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
gchar *feed_to_xml(gchar *key);
gboolean feed_new_from_xml(char *xml);
char *feeds_uid_from_xml (const char *xml);
void load_gconf_feed(void);
void migrate_old_config(gchar *feed_file);
void read_feeds(rssfeed *rf);
void get_feed_folders(void);
gchar *get_main_folder(void);
void migrate_crc_md5(const char *name, gchar *url);
