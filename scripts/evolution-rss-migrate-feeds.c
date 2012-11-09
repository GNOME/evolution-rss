/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2012 Lucian Langa <cooly@gnome.eu.org>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301 USA
 *
 *  This scripts help you if the 'magic' gsettings-data-convert fails to
 *  do its magic. This isn't actually a recommended procedure, it just
 *  blindly copies the enties in gconf over to dconf using gsettings.
 *  It requires gconf-2.0.pc to work.
 *
 *  To compile:
 *  	gcc -o evolution-rss-migrate-feeds evolution-rss-migrate-feeds.c \
 *  	`pkg-config --cflags --libs gtk+-2.0 gconf-2.0`
 */

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#define GCONF_SCHEMA "/apps/evolution/evolution-rss/feeds"
#define DCONF_SCHEMA "org.gnome.evolution.plugin.rss"

void
print_slist(gchar *s, GPtrArray *data)
{
	g_ptr_array_add(data, s);
}

int main(int argc, char **argv)
{
	GConfClient *client;
	GSList *list;

	gtk_init(&argc, &argv);
	
	client = gconf_client_get_default();
	list = gconf_client_get_list (client,
				GCONF_SCHEMA,
				GCONF_VALUE_STRING,
				NULL);
	GPtrArray *new = g_ptr_array_new();
	g_slist_foreach(list, (GFunc)print_slist, new);
	g_ptr_array_add(new, NULL);
	
	GSettings *settings= g_settings_new(DCONF_SCHEMA);
	g_settings_set_strv(settings, "feeds", (const gchar * const *)new->pdata);
	g_settings_sync();
}
