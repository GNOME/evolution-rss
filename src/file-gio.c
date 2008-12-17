/*  Evolution RSS Reader Plugin
 *  Copyright (C) 2007-2008 Lucian Langa <cooly@gnome.eu.org>
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


GString*
file_get_unblocking(const char *uri, GSList *headers, GString *post,
                  NetStatusCallback cb, gpointer data,
                  GError **err) {
	GFile *file;

	file = g_file_new_for_uri (uri);
	g_file_read_async (file,
                           G_PRIORITY_DEFAULT,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data);
	return 1;

#if LIBSOUP_VERSION < 2003000
        SoupUri *suri = NULL;
#else
        SoupURI *suri = NULL;
#endif
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
#if LIBSOUP_VERSION < 2003000
        g_signal_connect (soup_sess, "reauthenticate",
            G_CALLBACK (reauthenticate), soup_sess);
#endif

        req = soup_message_new(SOUP_METHOD_GET, url);
        if (!req)
        {
                g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
                                soup_status_get_phrase(2));                     //invalid url
                goto out;
        }
        d(g_print("request ok :%d\n", req->status_code));
        g_signal_connect(G_OBJECT(req), "got-chunk",
                        G_CALLBACK(got_chunk_blocking_cb), &info);
        for (; headers; headers = headers->next) {
                char *header = headers->data;
                /* soup wants the key and value separate, so we have to munge this
 *                  * a bit. */
                char *colonpos = strchr(header, ':');
                *colonpos = 0;
#if LIBSOUP_VERSION < 2003000
                soup_message_add_header(req->request_headers, header, colonpos+1);
#else
                soup_message_headers_append(req->request_headers, header, colonpos+1);
#endif
                *colonpos = ':';
        }
        gchar *agstr = g_strdup_printf("Evolution/%s; Evolution-RSS/%s",
                        EVOLUTION_VERSION_STRING, VERSION);
#if LIBSOUP_VERSION < 2003000
        soup_message_add_header (req->request_headers, "User-Agent",
                                agstr);
#else
        soup_message_headers_append (req->request_headers, "User-Agent",
                                agstr);
#endif
        g_free(agstr);

        proxify_session(soup_sess);
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

#if LIBSOUP_VERSION < 2003000
        response = g_string_new_len(req->response.body, req->response.length);
#else
        response = g_string_new_len(req->response_body->data, req->response_body->length);
#endif

out:
        if (suri) soup_uri_free(suri);
        if (req) g_object_unref(G_OBJECT(req));

        return response;
}

