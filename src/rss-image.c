/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2010 Lucian Langa <cooly@gnome.eu.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <camel/camel.h>

#if (DATASERVER_VERSION > 3005001)
#include <libedataserver/libedataserver.h>
#else
#include <libedataserver/e-data-server-util.h>
#endif

#include <mail/em-folder-tree.h>

#if (EVOLUTION_VERSION > 30501)
#include <em-format/e-mail-formatter.h>
#else
#include <mail/em-format-html.h>
#endif

#include <sys/time.h>
#include <string.h>

extern int rss_verbose_debug;

#include "fetch.h"
#include "misc.h"
#include "parser.h"
#include "rss.h"
#include "rss-cache.h"
#include "rss-config.h"
#include "rss-formatter.h"
#include "rss-image.h"
#include "rss-icon-factory.h"

extern gpointer current_pobject;
extern GtkTreeStore *evolution_store;
extern rssfeed *rf;
gchar *pixfile;
char *pixfilebuf;
gsize pixfilelen;
GHashTable *missing;

#if (EVOLUTION_VERSION >= 30400)
#include <mail/e-mail-reader.h>
#endif

void
#if LIBSOUP_VERSION < 2003000
finish_image_feedback (SoupMessage *msg, FEED_IMAGE *user_data);
#else
finish_image_feedback (SoupSession *soup_sess, SoupMessage *msg, FEED_IMAGE *user_data);
#endif

void
rss_init_images(void)
{
	/* load transparency */
	pixfile = g_build_filename (EVOLUTION_ICONDIR,
			"pix.png",
			NULL);
	g_file_load_contents (g_file_parse_name(pixfile),
			NULL,
			&pixfilebuf,
			&pixfilelen,
			NULL,
			NULL);
}

void
rss_finish_images(void)
{
	if (missing)
		g_hash_table_destroy(missing);
}

void
update_feed_image(RDF *r)
{
	GError *err = NULL;
	gchar *feed_file = NULL;
	gchar *key = gen_md5(r->uri);
	FEED_IMAGE *fi = g_new0(FEED_IMAGE, 1);
	gchar *image = r->image;
	gchar *feed_dir;

	if (!check_update_feed_image(key))
		goto out;
	feed_dir = rss_component_peek_base_directory();
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);
	feed_file = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s.img", feed_dir, key);
	d("feed_image() tmpurl:%s\n", feed_file);
	g_free(feed_dir);
	if (!g_file_test(feed_file, G_FILE_TEST_EXISTS)) {
	if (image) {		//we need to validate image here with load_pixbuf
		CamelStream *feed_fs = camel_stream_fs_new_with_name(feed_file,
			O_RDWR|O_CREAT, 0666, NULL);
		dup_auth_data(r->uri, image);
		fi->feed_fs = feed_fs;
		fi->key = g_strdup(key);
		d("call finish_create_icon_stream\n");
		fetch_unblocking(image,
			textcb,
			NULL,
			(gpointer)finish_create_icon_stream,
			fi,
			0,
			&err);
		if (err) {
			g_print("ERR:%s\n", err->message);
			goto out;
		}
	} else {
		gchar *server = get_server_from_uri(r->uri);
		//authentication data might be different
		dup_auth_data(r->uri, server);
		d("call finish_update_feed_image\n");
		fetch_unblocking(
			server,
			textcb,
			NULL,
			(gpointer)finish_update_feed_image,
			g_strdup(r->uri),// we need to dupe key here
			0,
			&err);		// because we might loose it if
		g_free(server);
					// feeds get deleted
	}
	}
out:	g_free(feed_file);
	g_free(key);
}

void
#if LIBSOUP_VERSION < 2003000
finish_update_feed_image (SoupMessage *msg, gpointer user_data)
#else
finish_update_feed_image (
	SoupSession *soup_sess, SoupMessage *msg, gpointer user_data)
#endif
{
	xmlChar *icon = NULL;
	gchar *icon_url = NULL;
	FEED_IMAGE *fi = NULL;
	gchar *feed_dir = rss_component_peek_base_directory();
	gchar *url = (gchar *)user_data;
	gchar *key = gen_md5(url);
	gchar *img_file = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s.img", feed_dir, key);
	gchar *urldir, *server;
	rfMessage *rfmsg;
	xmlChar *app;
	xmlNode *doc;

	g_free(feed_dir);
	sanitize_path_separator(img_file);
	urldir = g_path_get_dirname(url);
	server = get_server_from_uri(url);
	rfmsg = g_new0(rfMessage, 1);
	rfmsg->status_code = msg->status_code;
#if LIBSOUP_VERSION < 2003000
	rfmsg->body = msg->response.body;
	rfmsg->length = msg->response.length;
#else
	rfmsg->body = (gchar *)(msg->response_body->data);
	rfmsg->length = msg->response_body->length;
#endif
	doc = (xmlNode *)parse_html_sux (rfmsg->body, rfmsg->length);
	while (doc) {
		doc = html_find(doc, (gchar *)"link");
		if ((app = xmlGetProp(doc, (xmlChar *)"rel"))) {
			if (!g_ascii_strcasecmp((char *)app, "shorcut icon")
			|| !g_ascii_strcasecmp((char *)app, "icon")) {
				icon = xmlGetProp(doc, (xmlChar *)"href");
				break;
			}
		}
		xmlFree(app);
	}
	g_free(rfmsg);
	if (icon) {
		if (strstr((char *)icon, "://") == NULL)
			icon_url = g_strconcat(server, "/", icon, NULL);
		else
			icon_url = (char *)icon;

		dup_auth_data(url, g_strdup(icon_url));
		fi = g_new0(FEED_IMAGE, 1);
		fi->img_file = g_strdup(img_file);
		fi->key = g_strdup(key);
		fetch_unblocking(
			g_strdup(icon_url),
			textcb,
			NULL,
			(gpointer)finish_create_icon,
			fi,
			0,
//			&err);		// because we might lose it if
			NULL);
	} else {
		//              r->image = NULL;
		icon_url = g_strconcat(urldir, "/favicon.ico", NULL);
		dup_auth_data(url, g_strdup(icon_url));
		fi = g_new0(FEED_IMAGE, 1);
		fi->img_file = g_strdup(img_file);
		fi->key = g_strdup(key);
		fetch_unblocking(
				g_strdup(icon_url),
				textcb,
				NULL,
				(gpointer)finish_create_icon,
				fi,
				0,
//				&err);	// because we might lose it if
				NULL);
		g_free(icon_url);
		icon_url = g_strconcat(server, "/favicon.ico", NULL);
		dup_auth_data(url, g_strdup(icon_url));
		fi = g_new0(FEED_IMAGE, 1);
		fi->img_file = g_strdup(img_file);
		fi->key = g_strdup(key);
		fetch_unblocking(
				g_strdup(icon_url),
				textcb,
				NULL,
				(gpointer)finish_create_icon,
				fi,
				0,
//				&err);	// because we might lose it if
				NULL);
	}
	g_free(key);
	g_free(img_file);
	g_free(icon_url);
	g_free(server);
	g_free(urldir);
	g_free(user_data);
}

gboolean
check_update_feed_image(gchar *key)
{
	gchar *feed_dir = rss_component_peek_base_directory();
	gchar *fav_file = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s.fav", feed_dir, key);
	struct timeval start;
	FILE *f = NULL;
	gboolean ret = TRUE;
	unsigned long int remain;
	gchar rfeed[80];
	memset(rfeed, 0, 79);
	gettimeofday(&start, NULL);
	g_free(feed_dir);
	if (!g_file_test(fav_file, G_FILE_TEST_EXISTS)) {
		if ((f = fopen(fav_file, "w"))) {
			fprintf(f, "%lu", start.tv_sec);
			fclose(f);
		}
		ret = TRUE;
		goto out;
	}
	if ((f = fopen(fav_file, "r+"))) {
		fgets(rfeed, 50, f);
		remain = start.tv_sec - strtoul(
					(const char *)&rfeed, NULL, 10);
		if (FEED_IMAGE_TTL <= remain) {
			(void)fseek(f, 0L, SEEK_SET);
			fprintf(f, "%lu", start.tv_sec);
			fclose(f);
			ret =  TRUE;
			goto out;
		} else {
			d("next favicon will be fetched in %lu seconds\n",
				FEED_IMAGE_TTL - remain);
			fclose(f);
			ret = FALSE;
		}
	}
out:	g_free(fav_file);
	return ret;
}

void
#if LIBSOUP_VERSION < 2003000
finish_image_feedback (SoupMessage *msg, FEED_IMAGE *user_data)
#else
finish_image_feedback (SoupSession *soup_sess, SoupMessage *msg, FEED_IMAGE *user_data)
#endif
{
	RssCacheStream *stream = NULL;
	gchar *mime_type;
	d("finish_image_feedback()");
	stream = rss_cache_add(user_data->url);
	finish_image(soup_sess, msg, stream);
	if (!missing)
		missing = g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, NULL);

	mime_type = g_content_type_guess(NULL,
			(guchar *)msg->response_body->data,
			msg->response_body->length, NULL);

	if (503 == msg->status_code || //handle this timedly fasion
	    404 == msg->status_code || //NOT FOUND
	    400 == msg->status_code || //bad request
	      2 == msg->status_code || //STATUS_CANT_RESOLVE
	      1 == msg->status_code || //TIMEOUT (CANCELLED) ?
	      7 == msg->status_code ||// STATUS_IO_ERROR
		msg->response_body->length || //ZERO SIZE
	/*FIXME mime type here could be wrong */
	0 != g_ascii_strncasecmp (mime_type, "image/", 6)) { //ZERO SIZE
			g_hash_table_insert(missing,
				g_strdup(user_data->url),
				GINT_TO_POINTER(1));
		}
	g_free(mime_type);
	e_mail_display_load_images(user_data->data);
	g_free(user_data->url);
	g_free(user_data);
}

void
#if LIBSOUP_VERSION < 2003000
finish_image (SoupMessage *msg, RssCacheStream *user_data)
#else
finish_image (SoupSession *soup_sess, SoupMessage *msg, RssCacheStream *user_data)
#endif
{
	d("CODE:%d\n", msg->status_code);
	// we might need to handle more error codes here
	if (503 != msg->status_code && //handle this timedly fasion
	    404 != msg->status_code && //NOT FOUND
	    400 != msg->status_code && //bad request
	      2 != msg->status_code && //STATUS_CANT_RESOLVE
	      1 != msg->status_code && //TIMEOUT (CANCELLED) ?
	      7 != msg->status_code && // STATUS_IO_ERROR
#if LIBSOUP_VERSION < 2003000
		msg->response.length) {	//ZERO SIZE
#else
		msg->response_body->length) { //ZERO SIZE
#endif
#if LIBSOUP_VERSION < 2003000
		if (msg->response.body) {
			camel_stream_write(user_data,
				msg->response.body,
				msg->response.length);
#else
		if (msg->response_body->data) {
#if (DATASERVER_VERSION >= 3011002)
			GOutputStream *out;

			out = g_io_stream_get_output_stream (user_data);
			g_output_stream_write_all (out, 
				msg->response_body->data,
				msg->response_body->length,
				NULL,
				NULL,
				NULL);
			g_output_stream_flush (out, NULL, NULL);
			g_output_stream_close (out, NULL, NULL);
#else
#if (DATASERVER_VERSION >= 2033001)
			camel_stream_write (user_data,
				msg->response_body->data,
				msg->response_body->length,
				NULL,
				NULL);
			camel_stream_close(user_data, NULL, NULL);
#else
			camel_stream_write (user_data,
				msg->response_body->data,
				msg->response_body->length,
				NULL);
			camel_stream_close(user_data, NULL);
#endif
#endif
#endif
#if (DATASERVER_VERSION >= 2031001)
			g_object_unref(user_data);
#else
			camel_object_unref(user_data);
#endif
		}
	} else {
#if (DATASERVER_VERSION >= 3011002)
		GOutputStream *out;

		out = g_io_stream_get_output_stream (user_data);
		g_output_stream_write_all (out, 
			msg->response_body->data,
			msg->response_body->length,
			NULL,
			NULL,
			NULL);
		g_output_stream_flush (out, NULL, NULL);
		g_output_stream_close (out, NULL, NULL);
#else
#if (DATASERVER_VERSION >= 2033001)
		camel_stream_write (user_data, pixfilebuf, pixfilelen, NULL, NULL);
		camel_stream_close (user_data, NULL, NULL);
#else
		camel_stream_write(user_data, pixfilebuf, pixfilelen, NULL);
		camel_stream_close(user_data, NULL);
#endif
#endif

#if (DATASERVER_VERSION >= 2031001)
		g_object_unref(user_data);
#else
		camel_object_unref(user_data);
#endif
	}
}

static void
#if LIBSOUP_VERSION < 2003000
finish_image_camel (SoupMessage *msg, CamelStream *user_data)
#else
finish_image_camel (SoupSession *soup_sess, SoupMessage *msg, CamelStream *user_data)
#endif
{
	d("CODE:%d\n", msg->status_code);
	// we might need to handle more error codes here
	if (503 != msg->status_code && //handle this timedly fasion
	    404 != msg->status_code && //NOT FOUND
	    400 != msg->status_code && //bad request
	      2 != msg->status_code && //STATUS_CANT_RESOLVE
	      1 != msg->status_code && //TIMEOUT (CANCELLED) ?
	      7 != msg->status_code && // STATUS_IO_ERROR
#if LIBSOUP_VERSION < 2003000
		msg->response.length) {	//ZERO SIZE
#else
		msg->response_body->length) { //ZERO SIZE
#endif
#if LIBSOUP_VERSION < 2003000
		if (msg->response.body) {
			camel_stream_write(user_data,
				msg->response.body,
				msg->response.length);
#else
		if (msg->response_body->data) {
#if (DATASERVER_VERSION >= 2033001)
			camel_stream_write (user_data,
				msg->response_body->data,
				msg->response_body->length,
				NULL,
				NULL);
			camel_stream_close(user_data, NULL, NULL);
#else
			camel_stream_write (user_data,
				msg->response_body->data,
				msg->response_body->length,
				NULL);
			camel_stream_close(user_data, NULL);
#endif
#endif
#if (DATASERVER_VERSION >= 2031001)
			g_object_unref(user_data);
#else
			camel_object_unref(user_data);
#endif
		}
	} else {
#if (DATASERVER_VERSION >= 2033001)
		camel_stream_write (user_data, pixfilebuf, pixfilelen, NULL, NULL);
		camel_stream_close (user_data, NULL, NULL);
#else
		camel_stream_write(user_data, pixfilebuf, pixfilelen, NULL);
		camel_stream_close(user_data, NULL);
#endif

#if (DATASERVER_VERSION >= 2031001)
		g_object_unref(user_data);
#else
		camel_object_unref(user_data);
#endif
	}
}

void
#if LIBSOUP_VERSION < 2003000
finish_create_icon (SoupMessage *msg, FEED_IMAGE *user_data)
#else
finish_create_icon (SoupSession *soup_sess, SoupMessage *msg, FEED_IMAGE *user_data)
#endif
{
	d("finish_image(): status:%d, user_data:%s\n",
		msg->status_code, user_data->img_file);
	if (404 != msg->status_code) {
		CamelStream *feed_fs = camel_stream_fs_new_with_name(
					user_data->img_file,
					O_RDWR|O_CREAT, 0666, NULL);
		finish_image_camel(soup_sess, msg, feed_fs);
		display_folder_icon(evolution_store, user_data->key);
	}
	g_free(user_data->key);
	g_free(user_data);
}

void
#if LIBSOUP_VERSION < 2003000
finish_create_icon_stream (
	SoupMessage *msg, FEED_IMAGE *user_data)
#else
finish_create_icon_stream (
	SoupSession *soup_sess, SoupMessage *msg, FEED_IMAGE *user_data)
#endif
{
	finish_image_camel(soup_sess, msg, user_data->feed_fs);
	display_folder_icon(evolution_store, user_data->key);
	g_free(user_data->key);
	g_free(user_data);
}

gboolean
display_folder_icon(GtkTreeStore *tree_store, gchar *key)
{
	gchar *feed_dir = rss_component_peek_base_directory();
	gchar *img_file = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s.img", feed_dir, key);
	GdkPixbuf *icon, *pixbuf;
	gboolean result = FALSE;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeRowReference *row;
	EMFolderTreeModel *mod = (EMFolderTreeModel *)tree_store;
#if (EVOLUTION_VERSION < 31103)
	struct _EMFolderTreeModelStoreInfo *si;
#endif
	CamelStore *store = rss_component_peek_local_store();
	CamelFolderInfo *rssi = NULL;
	gint i=0, size;
	gint *sizes;

	if (!mod)
		return FALSE;

	pixbuf = gdk_pixbuf_new_from_file(img_file, NULL);

	if (pixbuf) {
		gchar *mf = get_main_folder();
		gchar *name = g_hash_table_lookup(rf->hrname_r, key);
		gchar *folder_name = lookup_feed_folder(name);
		gchar *full_name = g_build_path(
					G_DIR_SEPARATOR_S,
					mf,
					folder_name,
					NULL);
		g_free(folder_name);
		g_free(mf);
/*folder isn't created yet?*/
#if (DATASERVER_VERSION >= 2033001)
		rssi = camel_store_get_folder_info_sync (
					store,
					full_name, 0, NULL, NULL);
#else
		rssi = camel_store_get_folder_info (
					store,
					full_name, 0, NULL);
#endif
		if (!rssi) {
			g_free(full_name);
			result = FALSE;
			goto out;
		}
		icon = rss_build_icon (img_file, GTK_ICON_SIZE_MENU);
		d("icon:%p\n", icon);
		rss_append_folder_icons(g_strdup(key));
		sizes = gtk_icon_theme_get_icon_sizes(
				gtk_icon_theme_get_default(),
				"mail-read"); //will mail-read always be there?
		for (i=0; 0 != (size = sizes[i]); i++)
			d("icon set size:%d\n", size);
			gtk_icon_theme_add_builtin_icon(key,
				size,
				icon);
		g_free(sizes);

#if (EVOLUTION_VERSION < 31103)
		si = em_folder_tree_model_lookup_store_info (
			EM_FOLDER_TREE_MODEL (mod), store);
		row = g_hash_table_lookup (si->full_hash, full_name);
#else
		row = em_folder_tree_model_get_row_reference (
			EM_FOLDER_TREE_MODEL (mod), store, full_name);
#endif
		if (!row) goto out;
		path = gtk_tree_row_reference_get_path (row);
		gtk_tree_model_get_iter (
			(GtkTreeModel *)tree_store, &iter, path);
		gtk_tree_path_free (path);

		gtk_tree_store_set(
				tree_store, &iter,
				COL_STRING_ICON_NAME, key,
				-1);
		g_free(full_name);
#if (DATASERVER_VERSION >= 3011001)
		camel_folder_info_free (rssi);
#else
		camel_store_free_folder_info (store, rssi);
#endif
		g_object_unref(pixbuf);
		result = TRUE;
	}
out:	g_free(img_file);
	g_free(feed_dir);
	return result;
}

gchar *
decode_image_cache_filename(gchar *name)
{
	gsize size;
	gchar *csum, *tname;
	gchar *tmp;
	tmp = (gchar *)g_base64_decode(name+4, &size);
	csum = g_compute_checksum_for_string(G_CHECKSUM_SHA1,
			tmp, -1);
	g_free(tmp);
	tname = rss_cache_get_filename(csum);
	g_free(csum);
	return tname;
}

gboolean image_is_valid(gchar *image);

gboolean
file_is_image(gchar *image, gboolean cleanup);

gboolean
file_is_image(gchar *image, gboolean cleanup)
{
	gchar *mime_type, *contents;
	gsize length;
	gboolean result = TRUE;

	g_return_val_if_fail(image != NULL, FALSE);

	if (!g_file_test(image, G_FILE_TEST_EXISTS))
		return FALSE;

	/*need to get mime type via file contents or else mime type is
	 * bound to be wrong, especially on files fetched from the web
	 * this is very important as we might get quite a few images
	 * missing otherwise */
	g_file_get_contents (image,
		&contents,
		&length,
		NULL);
	mime_type = g_content_type_guess(NULL,
			(guchar *)contents, length, NULL);
	/*FIXME mime type here could be wrong */
	if (g_ascii_strncasecmp (mime_type, "image/", 6)) {
		struct stat st;
		if (cleanup) {
			g_stat(image, &st);
			if (st.st_size == 0)
				g_unlink(image);
		}
		result = FALSE;
	}
	g_free(mime_type);
	g_free(contents);
	return result;
}

/* validates if image is indeed an image file
 * if image file is not found it tries to fetch it
 * we need to check mime time against content
 * because we could end up with wrong file as image
 */
gchar *
verify_image(gchar *uri, EMailDisplay *format)
{
	gchar *nurl, *turl;
	gchar *feed_dir, *name;
#if (EVOLUTION_VERSION >= 30400)
	gchar *tmp = NULL;
#endif
	gchar *scheme, *tname;
	gchar *result = NULL;
	gchar *duri = NULL;

	if (!uri) return NULL;

	if (strstr(uri, "img:"))
		duri = decode_image_cache_filename(uri);
	else {
		if (!(duri = g_filename_from_uri(uri, NULL, NULL)))
			duri = g_strdup(uri);
	}

	if (!missing)
		missing = g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, NULL);

	if (g_file_test(duri, G_FILE_TEST_EXISTS)) {
		struct stat st;
		g_stat(duri, &st);
		if (st.st_size == 83) {
			if (g_hash_table_lookup(missing, g_path_get_basename (duri)))
				goto out;
			g_unlink(duri);
			d("retrying file:%s\n", duri);
		}
	}

out:	if (!g_file_test(duri, G_FILE_TEST_EXISTS)) {
			camel_url_decode((gchar *)uri);
			//FIXME lame method of extracting data cache path
			//there must be a function in camel for getting data cache path
			feed_dir = g_build_path(G_DIR_SEPARATOR_S,
				e_get_user_cache_dir(),
				"rss",
				NULL);
			scheme = g_uri_parse_scheme(uri);
			/* calling fetch_image_redraw with link NULL
			 * as we do not have base link here
			 * and not able to get it either
			 */
			if (!scheme) {
				nurl = strextr((gchar *)uri, feed_dir);
				g_free(feed_dir);
				turl = nurl + 4; // skip cache directory
				name = fetch_image_redraw(turl, NULL, format);
				g_free(nurl);
			} else {
				if (!strcmp(scheme, "file"))
					goto fail;
				turl = uri;
				name = fetch_image_redraw(uri, NULL, format);
				g_free(scheme);
			}
			if (name) {
				tname = decode_image_cache_filename(name);
				g_free(name);
#if (EVOLUTION_VERSION >= 23000)
#if (EVOLUTION_VERSION < 30400)
				result = g_filename_to_uri (tname, NULL, NULL);
#else
				tmp = g_filename_to_uri (tname, NULL, NULL);
				result = g_strconcat("evo-", tmp, NULL);
				g_free(tmp);
#endif
#else
				result = g_strdup(tname);
#endif
				if (!file_is_image(tname, 1)) {
					g_free(tname);
					goto fail;
				}
				g_free(tname);
			}

			if (duri)
				g_free(duri);
			return result;
	} else {
		if (!file_is_image(duri, 1))
			goto fail;
/*
 * appears the default has changed in efh_url_requested
 * the new default is file://
 * http://git.gnome.org/browse/evolution/commit/?id=d9deaf9bbc7fd9d0c72d5cf9b1981e3a56ed1162
 */
#if (EVOLUTION_VERSION >= 23000)
#if (EVOLUTION_VERSION < 30400)
		return g_filename_to_uri(duri?duri:uri, NULL, NULL);
#else
		return g_strconcat("evo-file://", duri?duri:uri, NULL);
#endif
#else
		return duri?duri:uri;
#endif
	}
fail:
#if (EVOLUTION_VERSION >= 23000)
#if (EVOLUTION_VERSION < 30400)
			result = g_filename_to_uri (pixfile, NULL, NULL);
#else
			result = g_strconcat("evo-file://", pixfile, NULL);
#endif
#else
			result = g_strdup(pixfile);
#endif
			if (duri)
				g_free(duri);
			return result;
}

// constructs url from @base in case url is relative
gchar *
fetch_image_redraw(gchar *url, gchar *link, gpointer data)
{
	GError *err = NULL;
	gchar *tmpurl = NULL;
	FEED_IMAGE *fi = NULL;
	gchar *result, *cache_file;
	gchar *intern, *burl;
	gsize size;

	g_return_val_if_fail(url != NULL, NULL);

	if (strstr(url, "img:"))
		tmpurl = (gchar *)g_base64_decode(url+4, &size);
	else {

	if (strstr(url, "://") == NULL) {
		if (*url == '.') //test case when url begins with ".."
			tmpurl = g_strconcat(
					g_path_get_dirname(link),
					"/", url, NULL);
		else {
			if (*url == '/')
				tmpurl = g_strconcat(
						get_server_from_uri(link),
						"/", url, NULL);
			else	//url is relative (does not begin with / or .)
				tmpurl = g_strconcat(
						g_path_get_dirname(link),
						"/", url, NULL);
		}
	} else {
		tmpurl = g_strdup(url);
	}

	if (!tmpurl)
		return NULL;
	}

	intern = g_compute_checksum_for_string(G_CHECKSUM_SHA1,
			tmpurl, -1);
	if (g_hash_table_find(rf->key_session,
			check_key_match,
			tmpurl)) {
		goto working;
	}
	cache_file = rss_cache_get_filename(intern);
	d("fetch_image_redraw() tmpurl:%s, intern: %s\n",
		tmpurl, cache_file);

	if (!g_file_test (cache_file, G_FILE_TEST_EXISTS)) {
		d("image cache MISS\n");
		if (data) {
			fi = g_new0(FEED_IMAGE, 1);
			fi->url = g_strdup(intern);
			fi->data = data;
			fetch_unblocking(tmpurl,
				textcb,
				g_strdup(tmpurl),
				(gpointer)finish_image_feedback,
				fi,
				1,
				&err);
		} else {
			RssCacheStream *stream = rss_cache_add(intern);
			fetch_unblocking(tmpurl,
				textcb,
				NULL,
				(gpointer)finish_image,
				stream,
				0,
				&err);
		}
		if (err) {
			result = NULL;
			g_free(cache_file);
			goto error;
		}
	} else {
		d("image cache HIT\n");
	}
	g_free(cache_file);

working:burl = (gchar *)g_base64_encode((guchar *)tmpurl, strlen(tmpurl));
	result = g_strdup_printf("img:%s", burl);
	g_free(burl);
error:	g_free(tmpurl);
	return result;
}

