/*  Evoution RSS Reader Plugin
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
 */

#ifndef __RSS_CONFIG_FACTORY_H_
#define __RSS_CONFIG_FACTORY_H_

#define SQLITE_MAGIC "SQLite format 3"

gboolean store_redraw(GtkTreeView *data);
void import_dialog_response(
	GtkWidget *selector,
	guint response,
	gpointer user_data);

void del_days_cb (GtkWidget *widget, add_feed *data);
void delete_feed_folder_alloc(gchar *old_name);

void rss_delete_folders (
	CamelStore *store,
	const char *full_name,
#if EVOLUTION_VERSION < 23191
	CamelException *ex);
#else
	GError **error);
#endif

void remove_feed_hash(gpointer name);
void init_rss_prefs(void);
void accept_cookies_cb(GtkWidget *widget, GtkWidget *data);
void del_messages_cb (GtkWidget *widget, add_feed *data);
void disable_widget_cb(GtkWidget *widget, GtkBuilder *data);
add_feed *build_dialog_add(gchar *url, gchar *feed_text);
void actions_dialog_add(add_feed *feed, gchar *url);
add_feed *create_dialog_add(gchar *url, gchar *feed_text);
void destroy_feed_hash_content(hrfeed *s);
hrfeed *save_feed_hash(gpointer name);
void restore_feed_hash(hrfeed *s);
void feeds_dialog_disable(GtkDialog *d, gpointer data);
GtkWidget *remove_feed_dialog(gchar *msg);
void feeds_dialog_delete(GtkDialog *d, gpointer data);
gchar *append_buffer(gchar *result, gchar *str);

void process_dialog_edit(
	add_feed *feed,
	gchar *url,
	gchar *feed_name);

void import_one_feed(
	gchar *url,
	gchar *title,
	gchar *prefix);

xmlNode *iterate_import_file(
		xmlNode *src,
		gchar **url,
		xmlChar **title,
		guint type);

#if LIBSOUP_VERSION >= 2026000
SoupCookieJar *import_cookies(gchar *file);
void process_cookies(SoupCookieJar *jar);
#endif
GtkWidget *create_import_cookies_dialog (void);

#endif /*__RSS_CONFIG_FACTORY_H_*/

