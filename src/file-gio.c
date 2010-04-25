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

typedef void (*UnblockCallback)(
			SoupSession *s,
			SoupMessage *m,
			gpointer user_data);

typedef struct FILE_GIO {
	UnblockCallback callback;
	gpointer callback_data;
} fg;

gboolean
file_get_unblocking(const char *uri, NetStatusCallback cb,
		gpointer data, gpointer cb2,
		gpointer cbdata2,
		guint track,
		GError **err)
{
	GFile *file;
	fg *FG = g_new0(fg, 1);
	/*repack user_data for gio_finish_feed*/
	FG->callback = cb2;
	FG->callback_data = cbdata2;

	file = g_file_new_for_uri (uri);
	g_file_load_contents_async (
		file,
		NULL,
		gio_finish_feed,
		FG);
	return 1;
}

void
gio_finish_feed (GObject *object, GAsyncResult *res, gpointer user_data)
{
	gsize file_size;
	char *file_contents;
	gboolean result;
	fg *FG = (fg *)user_data;

	SoupMessage *rfmsg = g_new0(SoupMessage, 1);

	result = g_file_load_contents_finish (
			G_FILE (object),
			res,
			&file_contents, &file_size,
			NULL, NULL);
	if (result) {
		rfmsg->status_code = SOUP_STATUS_OK;
		rfmsg->response_body = (SoupMessageBody *)g_string_new(NULL);
		rfmsg->response_body->data = file_contents;
		rfmsg->response_body->length = file_size;
		FG->callback(NULL, rfmsg, FG->callback_data);

		g_free (file_contents);
	}
	g_free(rfmsg);
}

