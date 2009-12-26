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

#include <gio/gio.h>
#include <libsoup/soup.h>

#include "rss.h"
#include "network.h"
#include "file-gio.h"

gboolean
file_get_unblocking(const char *uri, NetStatusCallback cb,
                                gpointer data, gpointer cb2,
                                gpointer cbdata2,
                                guint track,
                                GError **err)
{
	GFile *file;

	file = g_file_new_for_uri (uri);
	g_file_load_contents_async (file,
                           NULL,
                           cb2,
                           cbdata2);
	return 1;
}

void
gio_finish_feed (GObject *object, GAsyncResult *res, gpointer user_data)
{
        gsize file_size;
        char *file_contents;
        gboolean result;

        rfMessage *rfmsg = g_new0(rfMessage, 1);

        result = g_file_load_contents_finish (G_FILE (object),
                                              res,
                                              &file_contents, &file_size,
                                              NULL, NULL);
	if (result) {
        	rfmsg->status_code = SOUP_STATUS_OK;
        	rfmsg->body = file_contents;
        	rfmsg->length = file_size;
        	generic_finish_feed(rfmsg, user_data);
                g_free (file_contents);
        }
        g_free(rfmsg);
}

