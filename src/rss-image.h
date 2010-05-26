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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

typedef struct _FEED_IMAGE {
	gchar *img_file;
	CamelStream *feed_fs;
	gchar *url;
	gchar *key;
	gpointer data;
} FEED_IMAGE;

void rss_load_images(void);
gboolean display_folder_icon(GtkTreeStore *store, gchar *key);
gchar *decode_image_cache_filename(gchar *name);
void
#if LIBSOUP_VERSION < 2003000
finish_create_icon (SoupMessage *msg, FEED_IMAGE *user_data);
#else
finish_create_icon (SoupSession *soup_sess,
	SoupMessage *msg, FEED_IMAGE *user_data);
#endif
void
#if LIBSOUP_VERSION < 2003000
finish_create_icon_stream (SoupMessage *msg, FEED_IMAGE *user_data);
#else
finish_create_icon_stream (SoupSession *soup_sess,
	SoupMessage *msg, FEED_IMAGE *user_data);
#endif

gchar *verify_image(gchar *uri, EMFormatHTML *format);

void
#if LIBSOUP_VERSION < 2003000
finish_image (SoupMessage *msg, CamelStream *user_data);
#else
finish_image (SoupSession *soup_sess,
	SoupMessage *msg, CamelStream *user_data);
#endif
void
#if LIBSOUP_VERSION < 2003000
finish_create_image (SoupMessage *msg, gchar *user_data);
#else
finish_create_image (SoupSession *soup_sess,
	SoupMessage *msg, gchar *user_data);
#endif

