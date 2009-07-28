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
void import_dialog_response(GtkWidget *selector, guint response, gpointer user_data);
void del_days_cb (GtkWidget *widget, add_feed *data);
void delete_feed_folder_alloc(gchar *old_name);
void rss_delete_folders (CamelStore *store, const char *full_name, CamelException *ex);
void remove_feed_hash(gpointer name);

#endif /*__RSS_CONFIG_FACTORY_H_*/

