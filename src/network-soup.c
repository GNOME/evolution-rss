/*  Evolution RSS Reader Plugin
 *  Copyright (C) 2007-2010 Lucian Langa <cooly@gnome.eu.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gconf/gconf-client.h>
#ifdef HAVE_LIBSOUP_GNOME
#include <libsoup/soup-gnome.h>
#include <libsoup/soup-gnome-features.h>
#endif

#if (DATASERVER_VERSION >= 2023001)
#include <libedataserver/e-proxy.h>
#endif

extern int rss_verbose_debug;

#include "network.h"
#include "network-soup.h"
#include "rss.h"
#include "misc.h"

#define USE_PROXY FALSE

#define SS_TIMEOUT 30

#if LIBSOUP_VERSION > 2024000
SoupCookieJar *rss_soup_jar = NULL;
#endif
gint proxy_type = 0;
extern rssfeed *rf;
extern GConfClient *rss_gconf;
extern SoupSession *webkit_session;
#if (DATASERVER_VERSION >= 2023001)
EProxy *proxy;
#endif

typedef struct {
	NetStatusCallback user_cb;
	gpointer user_data;
	int current, total;
	gchar *chunk;
	gboolean reset;
	SoupSession *ss;
} CallbackInfo;

typedef struct {
	SoupSession *ss;
	SoupMessage *sm;
	gpointer cb2;
	gpointer cbdata2;
	gchar *url;
} STNET;

#define DOWNLOAD_QUEUE_SIZE 15
guint net_qid = 0;		// net queue dispatcher
guint net_queue_run_count = 0; //downloads in progress

static void
#if LIBSOUP_VERSION < 2003000
got_chunk_blocking_cb(SoupMessage *msg, CallbackInfo *info) {
#else
got_chunk_blocking_cb(SoupMessage *msg, SoupBuffer *chunk, CallbackInfo *info) {
#endif
	NetStatusProgress progress = {0};
	const char* clen;

	if (info->total == 0) {
#if LIBSOUP_VERSION < 2003000
		clen = soup_message_get_header(msg->response_headers,
			"Content-length");
#else
		clen = soup_message_headers_get(msg->response_headers,
			"Content-length");
#endif
		if (!clen)
			return;
		info->total = atoi(clen);
	}
#if LIBSOUP_VERSION < 2003000
	info->current += msg->response.length;
#else
	info->current += chunk->length;
#endif

	progress.current = info->current;
	progress.total = info->total;
	info->user_cb(NET_STATUS_PROGRESS, &progress, info->user_data);
}

static void
#if LIBSOUP_VERSION < 2003000
got_chunk_cb(SoupMessage *msg, CallbackInfo *info) {
#else
got_chunk_cb(SoupMessage *msg, SoupBuffer *chunk, CallbackInfo *info) {
#endif

	NetStatusProgress *progress = NULL;
	const char* clen;

	if (info->total == 0) {
#if LIBSOUP_VERSION < 2003000
		clen = soup_message_get_header(msg->response_headers,
				"Content-length");
			return;
#else
		clen = soup_message_headers_get(msg->response_headers,
				"Content-length");
#endif
		if (!clen)
			info->total = 0;
		else
			info->total = atoi(clen);
	}
#if LIBSOUP_VERSION < 2003000
	info->current += msg->response.length;
#else
	info->current += chunk->length;
#endif
	info->chunk = (gchar *)chunk->data;
	progress = g_new0(NetStatusProgress, 1);

	progress->current = info->current;
	progress->total = info->total;
	progress->chunk = (gchar *)chunk->data;
	progress->chunksize = (gint)chunk->length;
	if (info->reset) {
		progress->reset = info->reset;
		info->reset = 0;
	}
	info->user_cb(NET_STATUS_PROGRESS, progress, info->user_data);
	g_free(progress);
}

int net_error_quark(void)
{
	return 0;
}

void unblocking_error (SoupMessage *msg, gpointer user_data);

void
unblocking_error (SoupMessage *msg, gpointer user_data)
{
	g_print("data:%p\n", user_data);
}

void recv_msg (SoupMessage *msg, gpointer user_data);

void
recv_msg (SoupMessage *msg, gpointer user_data)
{
	GString *response = NULL;
#if LIBSOUP_VERSION < 2003000
	response = g_string_new_len(msg->response.body, msg->response.length);
#else
	response = g_string_new_len(msg->response_body->data, msg->response_body->length);
#endif
	d("got it!\n");
	d("res:[%s]\n", response->str);
}

static gboolean
remove_if_match (gpointer key, gpointer value, gpointer user_data)
{
	if (value == user_data)
	{
		g_hash_table_remove(rf->key_session, key);
		return TRUE;
	}
	else
		return FALSE;
}

void construct_abort(gpointer key, gpointer value, gpointer user_data);

void
construct_abort(gpointer key, gpointer value, gpointer user_data)
{
	g_hash_table_insert(rf->abort_session, key, value);
}

static void
unblock_free (gpointer user_data, GObject *ex_msg)
{
	d("weak ref - trying to free object\n");
	g_hash_table_remove(rf->session, user_data);
	g_hash_table_destroy(rf->abort_session);
	rf->abort_session = g_hash_table_new(g_direct_hash, g_direct_equal);
	g_hash_table_foreach(rf->session, construct_abort, NULL);
	g_hash_table_find(rf->key_session,
		remove_if_match,
		user_data);
	//this has been moved to soup-internal
	/*gboolean prune = soup_session_try_prune_connection (user_data);
	if (prune)
		g_object_unref(user_data);*/
	soup_session_abort (user_data);
}

#if (DATASERVER_VERSION >= 2023001)
EProxy *
proxy_init(void)
{
	EProxy *proxy;
	proxy = e_proxy_new ();
	e_proxy_setup_proxy (proxy);
	return proxy;
}

void
proxify_webkit_session(EProxy *proxy, gchar *uri)
{
	SoupURI *proxy_uri = NULL;
	gint ptype = gconf_client_get_int (rss_gconf, KEY_GCONF_EVO_PROXY_TYPE, NULL);

	switch (ptype) {
#ifndef HAVE_LIBSOUP_GNOME
	case 0:
#endif
	case 2:
		if (e_proxy_require_proxy_for_uri (proxy, uri)) {
#if (DATASERVER_VERSION >=2026000)
			proxy_uri = e_proxy_peek_uri_for (proxy, uri);
			d("webkit proxified %s with %s:%d\n", uri, proxy_uri->host, proxy_uri->port);
#else
			g_print("WARN: e_proxy_peek_uri_for() requires evolution-data-server 2.26\n");
			return;
#endif
		} else  {
			d("webkit no PROXY-%s\n", uri);
		}
		break;
		g_object_set (
			G_OBJECT (webkit_session),
			SOUP_SESSION_PROXY_URI, proxy_uri, NULL);
#ifdef HAVE_LIBSOUP_GNOME
	case 0:
		soup_session_add_feature_by_type (
			webkit_session, SOUP_TYPE_PROXY_RESOLVER_GNOME);
		break;
#endif
	}

}

//this will insert proxy in the session
void
proxify_session(EProxy *proxy, SoupSession *session, gchar *uri)
{
	SoupURI *proxy_uri = NULL;
	gint ptype = gconf_client_get_int (
			rss_gconf, KEY_GCONF_EVO_PROXY_TYPE, NULL);

	switch (ptype) {
#ifndef HAVE_LIBSOUP_GNOME
	case 0:
#endif
	case 2:
		if (e_proxy_require_proxy_for_uri (proxy, uri)) {
#if (DATASERVER_VERSION >=2026000)
			proxy_uri = e_proxy_peek_uri_for (proxy, uri);
#else
			g_print("WARN: e_proxy_peek_uri_for() requires evolution-data-server 2.26\n");
			return;
#endif
			if (proxy_uri) {
				d("proxified %s with %s:%d\n", uri, proxy_uri->host, proxy_uri->port);
			}
		} else {
			d("no PROXY-%s\n", uri);
		}
		g_object_set (
			G_OBJECT (session),
			SOUP_SESSION_PROXY_URI,
			proxy_uri, NULL);
		break;

#ifdef HAVE_LIBSOUP_GNOME
	case 0:
			soup_session_add_feature_by_type (
				session, SOUP_TYPE_PROXY_RESOLVER_GNOME);
		break;
#endif
	}

}
#endif

guint
read_up(gpointer data)
{
	FILE *fr;
	char rfeed[512];
	guint res = 0;
	gchar *tmp, *buf, *feed_dir, *feed_name;

	if (NULL != g_hash_table_lookup(rf->hruser, data))
		return 1;

	tmp = gen_md5(data);
	buf = g_strconcat(tmp, ".rec", NULL);
	g_free(tmp);

	feed_dir = rss_component_peek_base_directory();
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);
	feed_name = g_build_path(G_DIR_SEPARATOR_S, feed_dir, buf, NULL);
	g_free(feed_dir);
	d("reading auth info:%s\n", feed_name);

	fr = fopen(feed_name, "r");
	if (fr) {
		fgets(rfeed, 511, fr);
		g_hash_table_insert(
			rf->hruser, data, g_strstrip(g_strdup(rfeed)));
		fgets(rfeed, 511, fr);
		g_hash_table_insert(
			rf->hrpass, data, g_strstrip(g_strdup(rfeed)));
		fclose(fr);
		res = 1;
	}
	g_free(feed_name);
	g_free(buf);
	return res;
}

guint
save_up(gpointer data)
{
	FILE *fr;
	guint res = 0;
	gchar *feed_dir, *feed_name, *user, *pass;
	gchar *tmp = gen_md5(data);
	gchar *buf = g_strconcat(tmp, ".rec", NULL);
	g_free(tmp);

	feed_dir = rss_component_peek_base_directory();
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);
	feed_name = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", feed_dir, buf);
	g_free(feed_dir);

	fr = fopen(feed_name, "w+");
	if (fr) {
		user = g_hash_table_lookup(rf->hruser, data);
			fputs(user, fr);
		fputs("\n", fr);
		pass = g_hash_table_lookup(rf->hrpass, data);
		fputs(pass, fr);
		fclose(fr);
		res = 1;
	}
	g_free(feed_name);
	g_free(buf);
	return res;
}

guint
del_up(gpointer data)
{
	gchar *feed_dir, *feed_name;
	gchar *tmp = gen_md5(data);
	gchar *buf = g_strconcat(tmp, ".rec", NULL);
	g_free(tmp);
	feed_dir = rss_component_peek_base_directory();
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);
	feed_name = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", feed_dir, buf);
	g_free(feed_dir);
	unlink(feed_name);
	g_free(feed_name);
	g_free(buf);
	return 0;
}

static void
#if LIBSOUP_VERSION < 2003000
authenticate (SoupSession *session,
		SoupMessage *msg,
		const char *auth_type,
		const char *auth_realm,
		char **username,
		char **password,
		gpointer data)
#else
authenticate (SoupSession *session,
		SoupMessage *msg,
		SoupAuth *auth,
		gboolean retrying,
		gpointer data)
#endif
{
	SoupURI *proxy_uri;
	gchar *user, *pass;
	RSS_AUTH *auth_info = g_new0(RSS_AUTH, 1);

	if (msg->status_code == SOUP_STATUS_PROXY_UNAUTHORIZED) {
		g_print("proxy:%d\n", soup_auth_is_for_proxy(auth));
	g_object_get (G_OBJECT(session),
				"proxy-uri", &proxy_uri,
				NULL);
	//proxy_auth_dialog("Proxy Authentication", proxy_uri->user, proxy_uri->password);
	//g_object_set (G_OBJECT (session), SOUP_SESSION_PROXY_URI, proxy_uri, NULL);
	return;
	}

	user = g_hash_table_lookup(rf->hruser, data);
	pass = g_hash_table_lookup(rf->hrpass, data);
	d("data:%s, user:%s, pass:%s\n", (gchar *)data, user, pass);

	if (user && pass) {
#if LIBSOUP_VERSION < 2003000
		*username = g_strdup(user);
		*password = g_strdup(pass);
#else
	if (!retrying)
		soup_auth_authenticate (auth, user, pass);
	else {
		if (!rf->autoupdate)
			goto authpop;
	}
#endif
	} else {
		read_up(data);
		user = g_hash_table_lookup(rf->hruser, data);
		pass = g_hash_table_lookup(rf->hrpass, data);
		if (user && pass) {
#if LIBSOUP_VERSION < 2003000
			*username = g_strdup(user);
			*password = g_strdup(pass);
#else
			if (!retrying)
				soup_auth_authenticate (auth, user, pass);
			return;
#endif
		}
		//we test for autofetching in progresss because it seems
		//preety annoying to pop the authentication popup in front
		//of the user every time feeds are automatically fetched
		if (!rf->autoupdate) {
			//we will continue after user has made a decision on
			//web auth dialog
			//Bug 522147 – need to be able to pause synchronous I/O
authpop:		if (G_OBJECT_TYPE(session) == SOUP_TYPE_SESSION_ASYNC) {
				soup_session_pause_message(session, msg);
			}
			auth_info->url = data;
			auth_info->soup_auth = auth;
			auth_info->retrying = retrying;
			auth_info->session = session;
			auth_info->message = msg;
			web_auth_dialog(auth_info);
			return;
		}
	}
}

#if LIBSOUP_VERSION < 2003000
static void
reauthenticate (SoupSession *session,
		SoupMessage *msg,
		const char *auth_type,
		const char *auth_realm,
		char **username,
		char **password,
		gpointer data)
{
	if (rf->soup_auth_retry) {
		//means we're already tested once and probably
		//won't try again
		rf->soup_auth_retry = FALSE;
		if (create_user_pass_dialog(data)) {
			rf->soup_auth_retry = FALSE;
		} else {
			rf->soup_auth_retry = TRUE;
		}
		*username = g_strdup(g_hash_table_lookup(rf->hruser, data));
		*password = g_strdup(g_hash_table_lookup(rf->hrpass, data));
	}
}
#endif

guint net_get_status(const char *url, GError **err);

guint
net_get_status(const char *url, GError **err)
{
#if LIBSOUP_VERSION < 2003000
	SoupUri *suri = NULL;
#else
	SoupURI *suri = NULL;
#endif
	SoupMessage *req = NULL;
	guint response = 0;
	SoupSession *soup_sess = NULL;
	GSList *headers = NULL;
	gchar *agstr;

	if (!rf->b_session)
		rf->b_session = soup_sess =
			soup_session_sync_new_with_options(
				SOUP_SESSION_TIMEOUT, SS_TIMEOUT, NULL);
	else
		soup_sess = rf->b_session;

	req = soup_message_new(SOUP_METHOD_GET, url);
	if (!req) {
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC, "%s",
				soup_status_get_phrase(2));			//invalid url
		goto out;
	}
	for (; headers; headers = headers->next) {
		char *header = headers->data;
		/* soup wants the key and value separate, so we have to munge this
		 * a bit. */
		char *colonpos = strchr(header, ':');
		*colonpos = 0;
#if LIBSOUP_VERSION < 2003000
		soup_message_add_header(
			req->request_headers, header, colonpos+1);
#else
		soup_message_headers_append(
			req->request_headers, header, colonpos+1);
#endif
		*colonpos = ':';
	}
	agstr = g_strdup_printf("Evolution/%s; Evolution-RSS/%s",
			EVOLUTION_VERSION_STRING, VERSION);
#if LIBSOUP_VERSION < 2003000
	soup_message_add_header (req->request_headers,
		"User-Agent",
		agstr);
#else
	soup_message_headers_append (req->request_headers,
		"User-Agent",
		agstr);
#endif
	g_free(agstr);

	rf->b_session = soup_sess;
	rf->b_msg_session = req;
	soup_session_send_message(soup_sess, req);

	if (req->status_code != SOUP_STATUS_OK) {
		//might not be a good ideea
		soup_session_abort(soup_sess);
		g_object_unref(soup_sess);
		rf->b_session = NULL;
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC, "%s",
				soup_status_get_phrase(req->status_code));
		goto out;
	}

out:
	if (suri) soup_uri_free(suri);
	response = req->status_code;
	if (req) g_object_unref(G_OBJECT(req));

	return response;
}

static void
redirect_handler (SoupMessage *msg, gpointer user_data)
{
	if (SOUP_STATUS_IS_REDIRECTION (msg->status_code)) {
		CallbackInfo *info = user_data;
		SoupURI *new_uri;
		const gchar *new_loc;

		new_loc = soup_message_headers_get (msg->response_headers, "Location");
		if (!new_loc)
			return;

		info->reset=1;

		new_uri = soup_uri_new_with_base (soup_message_get_uri (msg), new_loc);
		if (!new_uri) {
			soup_message_set_status_full (msg,
				SOUP_STATUS_MALFORMED,
				"Invalid Redirect URL");
			return;
		}

		soup_message_set_uri (msg, new_uri);
		soup_session_requeue_message (info->ss, msg);

		soup_uri_free (new_uri);
	}
}


gboolean
net_get_unblocking(gchar *url,
			NetStatusCallback cb, gpointer data,
			gpointer cb2, gpointer cbdata2,
			guint track,
			GError **err)
{
	SoupMessage *msg;
	CallbackInfo *info = NULL;
	SoupSession *soup_sess;
	gchar *agstr;

	soup_sess = soup_session_async_new();


#if LIBSOUP_VERSION > 2024000
	if (rss_soup_jar) {
		soup_session_add_feature(
			soup_sess, SOUP_SESSION_FEATURE(rss_soup_jar));
	}
#endif

#if (DATASERVER_VERSION >= 2023001)
	proxify_session(proxy, soup_sess, url);
#endif
	if (cb && data) {
		info = g_new0(CallbackInfo, 1);
		info->user_cb = cb;
		info->user_data = data;
		info->current = 0;
		info->total = 0;
		info->ss = soup_sess;
	}

	g_signal_connect (soup_sess, "authenticate",
		G_CALLBACK (authenticate), (gpointer)url);
#if LIBSOUP_VERSION < 2003000
	g_signal_connect (soup_sess, "reauthenticate",
		G_CALLBACK (reauthenticate), (gpointer)url);
#endif

	/* Queue an async HTTP request */
	msg = soup_message_new ("GET", url);
	if (!msg) {
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC, "%s",
				soup_status_get_phrase(2));			//invalid url
		return FALSE;
	}

	if (track) {
		//we want to be able to abort this session by calling
		//abort_all_soup
		g_hash_table_insert(rf->session, soup_sess, msg);
		g_hash_table_insert(rf->abort_session, soup_sess, msg);
		g_hash_table_insert(rf->key_session, data, soup_sess);
	}

	agstr = g_strdup_printf("Evolution/%s; Evolution-RSS/%s",
			EVOLUTION_VERSION_STRING, VERSION);
#if LIBSOUP_VERSION < 2003000
	soup_message_add_header (msg->request_headers, "User-Agent",
		agstr);
#else
	soup_message_headers_append (msg->request_headers, "User-Agent",
		agstr);
#endif
	g_free(agstr);

	if (info) {
		g_signal_connect(G_OBJECT(msg), "got_chunk",
			G_CALLBACK(got_chunk_cb), info);	//FIXME Find a way to free this maybe weak_ref
		soup_message_set_flags (msg, SOUP_MESSAGE_NO_REDIRECT);
		soup_message_add_header_handler (msg, "got_body",
			"Location", G_CALLBACK (redirect_handler), info);
	}


	soup_session_queue_message (soup_sess, msg,
		cb2, cbdata2);

////	g_object_add_weak_pointer (G_OBJECT(msg), (gpointer)info);
	g_object_weak_ref (G_OBJECT(msg), unblock_free, soup_sess);
//	g_object_weak_ref (G_OBJECT(soup_sess), unblock_free, soup_sess);
//	GMainLoop *mainloop = g_main_loop_new (g_main_context_default (), FALSE);
//	g_timeout_add (10 * 1000, &conn_mainloop_quit, mainloop);
	return TRUE;
}


// same stuff as net_get_* but without accumulating headers
// push all donwloads to a customizable length queue
gboolean
download_unblocking(
	gchar *url,
	NetStatusCallback cb,
	gpointer data,
	gpointer cb2,
	gpointer cbdata2,
	guint track,
	GError **err)
{
	SoupMessage *msg;
	CallbackInfo *info = NULL;
	SoupSession *soup_sess;
	gchar *agstr;
	STNET *stnet;

	soup_sess = soup_session_async_new();


#if LIBSOUP_VERSION > 2024000
	if (rss_soup_jar) {
		soup_session_add_feature(soup_sess,
			SOUP_SESSION_FEATURE(rss_soup_jar));
	}
#endif

#if (DATASERVER_VERSION >= 2023001)
	proxify_session(proxy, soup_sess, url);
#endif
	if (cb && data) {
		info = g_new0(CallbackInfo, 1);
		info->user_cb = cb;
		info->user_data = data;
		info->current = 0;
		info->total = 0;
		info->ss = soup_sess;
	}

	g_signal_connect (soup_sess, "authenticate",
		G_CALLBACK (authenticate), (gpointer)url);
#if LIBSOUP_VERSION < 2003000
	g_signal_connect (soup_sess, "reauthenticate",
		G_CALLBACK (reauthenticate), (gpointer)url);
#endif

	/* Queue an async HTTP request */
	msg = soup_message_new ("GET", url);
	if (!msg) {
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC, "%s",
				soup_status_get_phrase(2));			//invalid url
		return FALSE;
	}

	if (track) {
		//we want to be able to abort this session by calling
		//abort_all_soup
		g_hash_table_insert(rf->session, soup_sess, msg);
		g_hash_table_insert(rf->abort_session, soup_sess, msg);
		g_hash_table_insert(rf->key_session, data, soup_sess);
	}

	agstr = g_strdup_printf("Evolution/%s; Evolution-RSS/%s",
			EVOLUTION_VERSION_STRING, VERSION);
#if LIBSOUP_VERSION < 2003000
	soup_message_add_header (msg->request_headers, "User-Agent",
		agstr);
#else
	soup_message_headers_append (msg->request_headers, "User-Agent",
		agstr);
#endif
	g_free(agstr);

	if (info) {
		g_signal_connect(G_OBJECT(msg), "got_chunk",
			G_CALLBACK(got_chunk_cb), info);	//FIXME Find a way to free this maybe weak_ref
	}

	soup_message_set_flags (msg, SOUP_MESSAGE_NO_REDIRECT);
	soup_message_add_header_handler (msg, "got_body",
		"Location", G_CALLBACK (redirect_handler), info);

	soup_message_body_set_accumulate (msg->response_body, FALSE);
	stnet = g_new0(STNET, 1);
	stnet->ss = soup_sess;
	stnet->sm = msg;
	stnet->cb2 = cb2;
	stnet->cbdata2 = cbdata2;
	stnet->url = g_strdup(url);
	g_queue_push_tail (rf->stqueue, stnet);
	rf->enclist = g_list_append (rf->enclist, g_strdup(url));
	if (!net_qid)
		net_qid = g_idle_add((GSourceFunc)net_queue_dispatcher, NULL);

////	g_object_add_weak_pointer (G_OBJECT(msg), (gpointer)info);
	g_object_weak_ref (G_OBJECT(msg), unblock_free, soup_sess);
//	g_object_weak_ref (G_OBJECT(soup_sess), unblock_free, soup_sess);
//	GMainLoop *mainloop = g_main_loop_new (g_main_context_default (), FALSE);
//	g_timeout_add (10 * 1000, &conn_mainloop_quit, mainloop);
	return TRUE;
}

gboolean
net_queue_dispatcher(void)
{
	STNET *_stnet;
	guint qlen = g_queue_get_length(rf->stqueue);

	d("que len:%d workers:%d\n",
		g_queue_get_length(rf->stqueue),
		net_queue_run_count);

	if (qlen && net_queue_run_count < gconf_client_get_int (
						rss_gconf,
						GCONF_KEY_DOWNLOAD_QUEUE_SIZE,
						NULL)) {
		net_queue_run_count++;
		_stnet = g_queue_pop_head(rf->stqueue);
		soup_session_queue_message (
			_stnet->ss,
			_stnet->sm,
			_stnet->cb2,
			_stnet->cbdata2);
		g_free(_stnet);
		return TRUE;
	}
	net_qid = 0;
	return FALSE;
}

GString*
net_post_blocking(gchar *url,
	GSList *headers,
	GString *post,
	NetStatusCallback cb,
	gpointer data,
	GError **err) {
#if LIBSOUP_VERSION < 2003000
	SoupUri *suri = NULL;
#else
	SoupURI *suri = NULL;
#endif
	SoupMessage *req = NULL;
	GString *response = NULL;
	CallbackInfo info = { cb, data, 0, 0 };
	SoupSession *soup_sess = NULL;
	gchar *agstr;

	if (!rf->b_session)
		rf->b_session = soup_sess =
			soup_session_sync_new_with_options(
				SOUP_SESSION_TIMEOUT,
				SS_TIMEOUT,
				NULL);
	else
		soup_sess = rf->b_session;


	g_signal_connect (soup_sess,
		"authenticate",
		G_CALLBACK (authenticate),
		(gpointer)url);
#if LIBSOUP_VERSION < 2003000
	g_signal_connect (soup_sess,
		"reauthenticate",
		G_CALLBACK (reauthenticate),
		(gpointer)url);
#endif

	req = soup_message_new(SOUP_METHOD_GET, url);
	if (!req) {
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC, "%s",
				soup_status_get_phrase(2));			//invalid url
		goto out;
	}
	d("request ok :%d\n", req->status_code);
	g_signal_connect(G_OBJECT(req), "got-chunk",
			G_CALLBACK(got_chunk_blocking_cb), &info);
	for (; headers; headers = headers->next) {
		char *header = headers->data;
		/* soup wants the key and value separate, so we have to munge this
		 * a bit. */
		char *colonpos = strchr(header, ':');
		*colonpos = 0;
#if LIBSOUP_VERSION < 2003000
		soup_message_add_header(
			req->request_headers,
			header,
			colonpos+1);
#else
		soup_message_headers_append(
			req->request_headers, header, colonpos+1);
#endif
		*colonpos = ':';
	}
	agstr = g_strdup_printf("Evolution/%s; Evolution-RSS/%s",
			EVOLUTION_VERSION_STRING, VERSION);
#if LIBSOUP_VERSION < 2003000
	soup_message_add_header (
		req->request_headers,
		"User-Agent",
		agstr);
#else
	soup_message_headers_append (
		req->request_headers,
		"User-Agent",
		agstr);
#endif
	g_free(agstr);

#if (DATASERVER_VERSION >= 2023001)
	proxify_session(proxy, soup_sess, url);
#endif
	rf->b_session = soup_sess;
	rf->b_msg_session = req;
	soup_session_send_message(soup_sess, req);

	if (req->status_code != SOUP_STATUS_OK) {
		//might not be a good ideea
		soup_session_abort(soup_sess);
		g_object_unref(soup_sess);
		rf->b_session = NULL;
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC, "%s",
				soup_status_get_phrase(req->status_code));
		goto out;
	}

#if LIBSOUP_VERSION < 2003000
	response = g_string_new_len(
			req->response.body,
			req->response.length);
#else
	response = g_string_new_len(
			req->response_body->data,
			req->response_body->length);
#endif

out:
	if (suri) soup_uri_free(suri);
	if (req) g_object_unref(G_OBJECT(req));

	return response;
}

gboolean
cancel_soup_sess(gpointer key, gpointer value, gpointer user_data)
{
	if (SOUP_IS_SESSION(key)) {
		soup_session_abort(key);
		g_hash_table_find(rf->key_session,
			remove_if_match,
			user_data);
	}
	return TRUE;
}

void remove_weak(gpointer key, gpointer value, gpointer user_data);

void
remove_weak(gpointer key, gpointer value, gpointer user_data)
{
	g_object_weak_unref(value, unblock_free, key);
}

void
abort_all_soup(void)
{
	//abort all session
	rf->cancel = 1;
	rf->cancel_all = 1;
	if (rf->abort_session) {
		g_hash_table_foreach(rf->abort_session, remove_weak, NULL);
		g_hash_table_foreach_remove(
			rf->abort_session, cancel_soup_sess, NULL);
//              g_hash_table_foreach(rf->abort_session, cancel_soup_sess, NULL);
		g_hash_table_destroy(rf->session);
		rf->session = g_hash_table_new(
				g_direct_hash, g_direct_equal);
	}
	if (rf->progress_bar) {
		gtk_progress_bar_set_fraction(
			(GtkProgressBar *)rf->progress_bar, 1);
		rf->progress_bar = NULL;        //there's no need to update bar once we canceled feeds
	}
	if (rf->b_session) {
		soup_session_abort(rf->b_session);
		rf->b_session = NULL;
		rf->b_msg_session = NULL;
	}
	rf->cancel = 0;
	rf->cancel_all = 0;
}

void
sync_gecko_cookies(void)
{
	//this currently sux as firefox 3.5b will open
	//cookie database file exclusively, that means import will fail
	//even fetch will fail - we should copy this file separately for
	//gecko renderer
	//symlink(cookie_path, moz_cookie_path);
	GFile *cookie_file, *moz_cookie_file;
	gchar *feed_dir = rss_component_peek_base_directory();
	gchar *cookie_path = g_build_path(
				G_DIR_SEPARATOR_S, feed_dir,
				"rss-cookies.sqlite", NULL);
	gchar *moz_cookie_path = g_build_path(
				G_DIR_SEPARATOR_S, feed_dir,
				"mozembed-rss", "cookies.sqlite", NULL);

	cookie_file = g_file_new_for_path (cookie_path);
	moz_cookie_file = g_file_new_for_path (moz_cookie_path);
	g_file_copy(cookie_file, moz_cookie_file,
			G_FILE_COPY_OVERWRITE,
			NULL, NULL, NULL, NULL);
	g_free(cookie_path);
	g_free(moz_cookie_path);
}

void
rss_soup_init(void)
{
#if LIBSOUP_VERSION > 2026002 && defined(HAVE_LIBSOUP_GNOME)
	if (gconf_client_get_bool (rss_gconf, GCONF_KEY_ACCEPT_COOKIES, NULL)) {
		gchar *feed_dir = rss_component_peek_base_directory();
		gchar *cookie_path = g_build_path(
					G_DIR_SEPARATOR_S,
					feed_dir,
					"rss-cookies.sqlite",
					NULL);
		gchar *moz_cookie_path = g_build_path(
						G_DIR_SEPARATOR_S,
						feed_dir,
						"mozembed-rss",
						"cookies.sqlite",
						NULL);
		g_free(feed_dir);

		rss_soup_jar =
			soup_cookie_jar_sqlite_new (cookie_path, FALSE);

		if (!g_file_test(moz_cookie_path, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_SYMLINK)) {
			sync_gecko_cookies();
		}
		g_free(cookie_path);
		g_free(moz_cookie_path);
	}
#endif
	if (!rf->stqueue)
		rf->stqueue = g_queue_new();
}
