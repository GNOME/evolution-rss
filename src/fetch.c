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

#include "network.h"

GString*
fetch_blocking(const char *url, GSList *headers, GString *post,
                  NetStatusCallback cb, gpointer data,
                  GError **err) {

	gchar *scheme = NULL;
	GString *result = NULL;
	
	scheme = g_uri_parse_scheme(url);
	d(g_print("scheme:%s\n", scheme));
	if (!g_ascii_strcasecmp(scheme, "file")) {
		gchar *fname = g_filename_from_uri(url, NULL, NULL);
		FILE *f = g_fopen(fname, "rb");
		g_free(fname);
		g_free(scheme);
	 	if (f == NULL)
                	goto error;	
		gchar *buf = g_new0 (gchar, 4096);
		result = g_string_new(NULL);
		while (fgets(buf, 4096, f) != NULL) {
			g_string_append_len(result, buf, strlen(buf));
		}
		fclose(f);
		return result;
	} else {
		g_free(scheme);
        	return net_post_blocking(url, NULL, post, cb, data, err);
	}
error:
	g_print("error\n");
	g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
                                g_strerror(errno));
	return result;
}

gboolean
fetch_unblocking(const char *url, NetStatusCallback cb, gpointer data, 
				gpointer cb2, gpointer cbdata2,
				guint track,
				GError **err)
{
	gchar *scheme = NULL;
	scheme = g_uri_parse_scheme(url);

	if (!g_ascii_strcasecmp(scheme, "file")) {
		
	} else {
		return net_get_unblocking(url,
                                cb,
                                NULL,
                                cb2,
                                cbdata2,
                                0,
                                &err);
	}
}

