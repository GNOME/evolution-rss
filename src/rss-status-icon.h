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

typedef struct {
	gchar *chn_name;
	gchar *title;
} StatusText;

void icon_activated (GtkStatusIcon *icon, gpointer pnotify);
gboolean button_press_cb (GtkWidget *widget, GdkEventButton *event, gpointer data);
void create_status_icon(void);
gboolean flicker_stop(gpointer user_data);
void flatten_status(StatusText *st, gchar **user_data);
void toggle_window(void);
void update_status_icon(GQueue *status);
void update_status_icon_text(GQueue *status_msg, const char *channel, gchar *title);
