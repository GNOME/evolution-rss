/*  Evolution RSS Reader Plugin
 *  Copyright (C) 2007  Lucian Langa <cooly@mips.edu.ms>
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

#include <libsoup/soup.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>

#include "network.h"
#include "rss.h"
#include "misc.c"

#define USE_PROXY FALSE

#define SS_TIMEOUT 30

extern rssfeed *rf;

typedef struct {
	NetStatusCallback user_cb;
	gpointer user_data;
	int current, total;
} CallbackInfo;

static void
got_chunk_blocking_cb(SoupMessage *msg, CallbackInfo *info) {
    NetStatusProgress progress = {0};
    const char* clen;

    if (info->total == 0) {
        clen = soup_message_get_header(msg->response_headers,
                "Content-length");
        if (!clen)
            return;
        info->total = atoi(clen);
    }
    info->current += msg->response.length;

    progress.current = info->current;
    progress.total = info->total;
    info->user_cb(NET_STATUS_PROGRESS, &progress, info->user_data);
}

static void
got_chunk_cb(SoupMessage *msg, CallbackInfo *info) {
	NetStatusProgress *progress = NULL;
	const char* clen;
	
	if (info->total == 0) {
		clen = soup_message_get_header(msg->response_headers,
				"Content-length");
		if (!clen)
			return;
		info->total = atoi(clen);
	}
	info->current += msg->response.length;
	progress = g_new0(NetStatusProgress, 1);

	progress->current = info->current;
	progress->total = info->total;
	info->user_cb(NET_STATUS_PROGRESS, progress, info->user_data);
	g_free(progress);
}

int net_error_quark(void);
#define NET_ERROR net_error_quark()

int net_error_quark(void)
{
	return 0;
}

void
unblocking_error (SoupMessage *msg, gpointer user_data)
{
	g_print("data:%p\n", user_data);
}

void
recv_msg (SoupMessage *msg, gpointer user_data)
{
	GString *response = NULL;
	response = g_string_new_len(msg->response.body, msg->response.length);
#ifdef RSS_DEBUG
	g_print("got it!\n");
	g_print("res:[%s]\n", response->str);
#endif
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

static void
unblock_free (gpointer user_data, GObject *ex_msg)
{
#ifdef RSS_DEBUG
	g_print("weak ref - trying to free object\n");
#endif
	gboolean prune = soup_session_try_prune_connection (user_data);
	//I really don't know if his is necesarry
	//but I believe it won't hurt
	if (prune)
		g_object_unref(user_data);
	g_hash_table_remove(rf->session, user_data);
	g_hash_table_find(rf->key_session,
		remove_if_match,
		user_data);
}

//this will insert proxy in the session
void
proxyfy_session(SoupSession *session)
{
	gboolean use_proxy =
       	gconf_client_get_bool(rss_gconf, GCONF_KEY_USE_PROXY, NULL);
    gint port_proxy =
        gconf_client_get_int(rss_gconf, GCONF_KEY_PORT_PROXY, NULL);
    gchar *host_proxy =
        gconf_client_get_string(rss_gconf, GCONF_KEY_HOST_PROXY, NULL);
	gboolean auth_proxy =
        gconf_client_get_bool(rss_gconf, GCONF_KEY_AUTH_PROXY, NULL);
    gchar *user_proxy =
        gconf_client_get_string(rss_gconf, GCONF_KEY_USER_PROXY, NULL);
    gchar *pass_proxy =
        gconf_client_get_string(rss_gconf, GCONF_KEY_PASS_PROXY, NULL);


    if (use_proxy && host_proxy && port_proxy > 0)
    {
        gchar *proxy_uri = 
            g_strdup_printf("http://%s:%d/", host_proxy, port_proxy); 

        SoupUri *puri = soup_uri_new (proxy_uri);
	if (auth_proxy)
	{
		puri->user = g_strdup(user_proxy);
		puri->passwd = g_strdup(pass_proxy);
	}
        g_object_set (G_OBJECT (session), SOUP_SESSION_PROXY_URI, puri, NULL);
        if (puri)
            g_free(puri);
        if (proxy_uri)
            g_free(proxy_uri);
    }
}

guint
read_up(gpointer data)
{
	char rfeed[512];
	gchar *tmp = gen_md5(data);
	gchar *buf = g_strconcat(tmp, ".rec", NULL);
	g_free(tmp);
	guint res = 0;

	gchar *feed_dir = g_strdup_printf("%s/mail/rss",
            mail_component_peek_base_directory (mail_component_peek ()));
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
            g_mkdir_with_parents (feed_dir, 0755);

	gchar *feed_name = g_strdup_printf("%s/%s", feed_dir, buf);
	g_free(feed_dir);

	FILE *fr = fopen(feed_name, "r");
	if (fr)
	{
        	fgets(rfeed, 511, fr);
        	g_hash_table_insert(rf->hruser, data, g_strstrip(g_strdup(rfeed)));
        	fgets(rfeed, 511, fr);
        	g_hash_table_insert(rf->hrpass, data, g_strstrip(g_strdup(rfeed)));
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
	gchar *tmp = gen_md5(data);
	gchar *buf = g_strconcat(tmp, ".rec", NULL);
	g_free(tmp);
	guint res = 0;

	gchar *feed_dir = g_strdup_printf("%s/mail/rss",
            mail_component_peek_base_directory (mail_component_peek ()));
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
            g_mkdir_with_parents (feed_dir, 0755);

	gchar *feed_name = g_strdup_printf("%s/%s", feed_dir, buf);
	g_free(feed_dir);

	FILE *fr = fopen(feed_name, "w+");
	if (fr)
	{
        	gchar *user = g_hash_table_lookup(rf->hruser, data);
			fputs(user, fr);
	        fputs("\n", fr);
        	gchar *pass = g_hash_table_lookup(rf->hrpass, data);
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
	gchar *tmp = gen_md5(data);
	gchar *buf = g_strconcat(tmp, ".rec", NULL);
	g_free(tmp);
	gchar *feed_dir = g_strdup_printf("%s/mail/rss",
            mail_component_peek_base_directory (mail_component_peek ()));
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
            g_mkdir_with_parents (feed_dir, 0755);

	gchar *feed_name = g_strdup_printf("%s/%s", feed_dir, buf);
	g_free(feed_dir);
	unlink(feed_name);
	g_free(feed_name);
	g_free(buf);
	return 0;
}

static void
authenticate (SoupSession *session,
        SoupMessage *msg,
        const char *auth_type,
        const char *auth_realm,
        char **username,
        char **password,
        gpointer data)
{
	gchar *user = g_hash_table_lookup(rf->hruser, data);
	gchar *pass = g_hash_table_lookup(rf->hrpass, data);
	if (user && pass)
	{
		*username = g_strdup(user);
		*password = g_strdup(pass);
	}
	else
	{
		if (!read_up(data))
			create_user_pass_dialog(data);

		user = g_hash_table_lookup(rf->hruser, data);
		pass = g_hash_table_lookup(rf->hrpass, data);
		*username = g_strdup(user);
		*password = g_strdup(pass);
	}
}
	
static void
reauthenticate (SoupSession *session,
        SoupMessage *msg,
        const char *auth_type,
        const char *auth_realm,
        char **username,
        char **password,
        gpointer data)
{
		create_user_pass_dialog(data);
        *username = g_strdup(g_hash_table_lookup(rf->hruser, data));
        *password = g_strdup(g_hash_table_lookup(rf->hrpass, data));
}

static int
conn_mainloop_quit (void *data)
{
	g_print("loop quit");
  g_main_loop_quit (data);
}

gboolean
net_get_unblocking(const char *url, NetStatusCallback cb, 
				gpointer data, gpointer cb2,
				gpointer cbdata2,
				GError **err)
{
	SoupMessage *msg;
	CallbackInfo *info;
	SoupSession *soup_sess = 
//		soup_session_async_new_with_options(SOUP_SESSION_TIMEOUT, SS_TIMEOUT, NULL);
		soup_session_async_new();
			
	proxyfy_session(soup_sess);
	info = g_new0(CallbackInfo, 1);
	info->user_cb = cb;
	info->user_data = data;
	info->current = 0;
	info->total = 0;
	if (!rf->session)
		rf->session = g_hash_table_new(g_direct_hash, g_direct_equal);
	if (!rf->key_session)
		rf->key_session = g_hash_table_new(g_direct_hash, g_direct_equal);

	g_signal_connect (soup_sess, "authenticate",
            G_CALLBACK (authenticate), (gpointer)url);
	g_signal_connect (soup_sess, "reauthenticate",
            G_CALLBACK (reauthenticate), (gpointer)url);

	/* Queue an async HTTP request */
	msg = soup_message_new ("GET", url);
	if (!msg)
	{
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
				soup_status_get_phrase(2));			//invalid url
		return -1;
	}
	g_hash_table_insert(rf->session, soup_sess, msg);
	g_hash_table_insert(rf->key_session, data, soup_sess);

	gchar *agstr = g_strdup_printf("Evolution/%s; Evolution-RSS/%s",
			EVOLUTION_VERSION_STRING, VERSION);
	soup_message_add_header (msg->request_headers, "User-Agent",
                                agstr);
	g_free(agstr);

	g_signal_connect(G_OBJECT(msg), "got-chunk",
			G_CALLBACK(got_chunk_cb), info);	//FIXME Find a way to free this maybe weak_ref

	soup_session_queue_message (soup_sess, msg,
           cb2, cbdata2);

	g_object_add_weak_pointer (G_OBJECT(msg), (gpointer)info);
	g_object_weak_ref (G_OBJECT(msg), unblock_free, soup_sess);
//	GMainLoop *mainloop = g_main_loop_new (g_main_context_default (), FALSE);
  //	g_timeout_add (10 * 1000, &conn_mainloop_quit, mainloop);
	return 1;
}

GString*
net_post_blocking(const char *url, GSList *headers, GString *post,
                  NetStatusCallback cb, gpointer data,
                  GError **err) {
	SoupUri *suri = NULL;
	SoupMessage *req = NULL;
	GString *response = NULL;
	CallbackInfo info = { cb, data, 0, 0 };
	SoupSession *soup_sess = NULL;

	if (!rf->b_session)
		rf->b_session = soup_sess = 
			soup_session_sync_new_with_options(SOUP_SESSION_TIMEOUT, SS_TIMEOUT, NULL);
	else
		soup_sess = rf->b_session;

	g_signal_connect (soup_sess, "authenticate",
            G_CALLBACK (authenticate), soup_sess);
	g_signal_connect (soup_sess, "reauthenticate",
            G_CALLBACK (reauthenticate), soup_sess);

	suri = soup_uri_new(url);
	if (!suri)
	{
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
				soup_status_get_phrase(2));			//invalid url
		goto out;
	}
	req = soup_message_new_from_uri(SOUP_METHOD_GET, suri);
	g_signal_connect(G_OBJECT(req), "got-chunk",
			G_CALLBACK(got_chunk_blocking_cb), &info);
	for (; headers; headers = headers->next) {
		char *header = headers->data;
		/* soup wants the key and value separate, so we have to munge this
		 * a bit. */
		char *colonpos = strchr(header, ':');
		*colonpos = 0;
		soup_message_add_header(req->request_headers, header, colonpos+1);
		*colonpos = ':';
	}
	gchar *agstr = g_strdup_printf("Evolution/%s; Evolution-RSS/%s",
			EVOLUTION_VERSION_STRING, VERSION);
	soup_message_add_header (req->request_headers, "User-Agent",
                                agstr);
	g_free(agstr);

	proxyfy_session(soup_sess);
	rf->b_session = soup_sess;
	rf->b_msg_session = req;
	soup_session_send_message(soup_sess, req);

	if (req->status_code != SOUP_STATUS_OK) {
		//might not be a good ideea
		soup_session_abort(soup_sess);
		g_object_unref(soup_sess);
		rf->b_session = NULL;
		g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
				soup_status_get_phrase(req->status_code));
		goto out;
	}

	response = g_string_new_len(req->response.body, req->response.length);

out:
	if (suri) soup_uri_free(suri);
	if (req) g_object_unref(G_OBJECT(req));
	
	return response;
}

