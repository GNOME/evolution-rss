/*  Evolution RSS Reader Plugin
 *  Copyright (C) 2007-2009 Lucian Langa <cooly@gnome.eu.org>
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

#include <stdio.h>
#include <string.h>
#include <errno.h>

extern int rss_verbose_debug;

#include "network.h"
#include "file-gio.h"
#include "network-soup.h"
#include "fetch.h"
#include "debug.h"

GString*
fetch_blocking(gchar *url, GSList *headers, GString *post,
	NetStatusCallback cb, gpointer data,
	GError **err) {

	gchar *scheme = NULL;
	gchar *buf, *fname;
	GString *result = NULL;
	FILE *f = NULL;

	scheme = g_uri_parse_scheme(url);
	if (scheme && !g_ascii_strcasecmp(scheme, "file")) {
		fname = g_filename_from_uri(url, NULL, NULL);
		f = fopen(fname, "rb");
		g_free(fname);
		g_free(scheme);
		if (f == NULL)
			goto error;
		buf = g_new0 (gchar, 4096);
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
	g_set_error(
		err,
		NET_ERROR,
		NET_ERROR_GENERIC,
		"%s",
		g_strerror(errno));
	return result;
}

//fetch feed
//FIXME gio callback hardcoded
// data - also used as key in key_session when track = 1

gboolean
fetch_unblocking(gchar *url, NetStatusCallback cb, gpointer data,
				gpointer cb2, gpointer cbdata2,
				guint track,
				GError **err)
{
	gchar *scheme = NULL;
	scheme = g_uri_parse_scheme(g_strstrip(url));
	d("scheme:%s=>url:%s\n", scheme, url);

	if (!scheme)
		return FALSE;

	if (!g_ascii_strcasecmp(scheme, "file")) {
		g_free(scheme);
		return file_get_unblocking(url,
				NULL, // add status here //
				NULL,
				cb2,
				cbdata2,
				0,
				err);
	} else {
		g_free(scheme);
		return net_get_unblocking(url,
				cb,
				data,
				cb2,
				cbdata2,
				track,
				err);
	}
}

