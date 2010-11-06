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

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "rss.h"
#include "rss-status-icon.h"

GtkStatusIcon *status_icon = NULL;
gboolean winstatus;
gchar *flat_status_msg;
extern GtkWidget *evo_window;
extern GConfClient *rss_gconf;
extern GQueue *status_msg;

void
icon_activated (GtkStatusIcon *icon, gpointer pnotify)
{
	gchar *uri;
	gchar *real_name;
	gchar *iconfile = g_build_filename (EVOLUTION_ICONDIR,
				"rss-icon-read.png",
				NULL);
	gtk_status_icon_set_from_file (
		status_icon,
		iconfile);
	g_free(iconfile);
	gtk_status_icon_set_has_tooltip (status_icon, FALSE);
	uri = g_object_get_data (G_OBJECT (status_icon), "uri");
	if (uri) {
		real_name = g_build_path(G_DIR_SEPARATOR_S,
				lookup_main_folder(),
				lookup_feed_folder(uri), NULL);
		rss_select_folder(real_name);
	}
}

gboolean
button_press_cb (
	GtkWidget *widget,
	GdkEventButton *event,
	gpointer data)
{
	if (((event->button != 1) || (event->type != GDK_2BUTTON_PRESS)) && winstatus != TRUE) {
		return FALSE;
	}
	toggle_window();
	icon_activated(NULL, NULL);
	return TRUE;
}

void
create_status_icon(void)
{
	if (!status_icon) {
		gchar *iconfile = g_build_filename (EVOLUTION_ICONDIR,
					"rss-icon-read.png",
					NULL);
		status_icon = gtk_status_icon_new ();
		gtk_status_icon_set_from_file (
			status_icon,
			iconfile);
		g_free(iconfile);
/*		g_signal_connect (
			G_OBJECT (status_icon),
			"activate",
			G_CALLBACK (icon_activated),
			NULL);*/
		g_signal_connect (
			G_OBJECT (status_icon),
			"button-press-event",
			G_CALLBACK (button_press_cb),
			NULL);
	}
	gtk_status_icon_set_has_tooltip (status_icon, FALSE);
}

gboolean
flicker_stop(gpointer user_data)
{
#if GTK_MINOR_VERSION < 22
	gtk_status_icon_set_blinking (status_icon, FALSE);
#endif
	return FALSE;
}


void
flaten_status(gpointer msg, gpointer user_data)
{
	if (strlen(msg)) {
		if (flat_status_msg)
			flat_status_msg = g_strconcat(
						flat_status_msg,
						msg,
						NULL);
		else
			flat_status_msg = g_strdup(msg);
	}
}

void
toggle_window(void)
{
#if EVOLUTION_VERSION < 22900 //KB//
	GList *p, *pnext;
	for (p = (gpointer)evo_window; p != NULL; p = pnext) {
		pnext = p->next;

		if (gtk_window_is_active(GTK_WINDOW(p->data))) {
			gtk_window_iconify(GTK_WINDOW(p->data));
			gtk_window_set_skip_taskbar_hint(
				GTK_WINDOW(p->data), TRUE);
			winstatus = TRUE;
		} else {
			gtk_window_iconify(GTK_WINDOW(p->data));
			evo_window_popup(GTK_WIDGET(p->data));
			gtk_window_set_skip_taskbar_hint(
				GTK_WINDOW(p->data), FALSE);
			winstatus = FALSE;
		}
	}
#else
	if (gtk_window_is_active(GTK_WINDOW(evo_window))) {
		gtk_window_iconify(GTK_WINDOW(evo_window));
		gtk_window_set_skip_taskbar_hint(
			GTK_WINDOW(evo_window), TRUE);
		winstatus = TRUE;
	} else {
		gtk_window_iconify(GTK_WINDOW(evo_window));
		evo_window_popup(GTK_WIDGET(evo_window));
		gtk_window_set_skip_taskbar_hint(
			GTK_WINDOW(evo_window), FALSE);
		winstatus = FALSE;
	}
#endif
}

void
update_status_icon(const char *channel, gchar *title)
{
	gchar *total;
	gchar *iconfile;
	gchar *tchn, *ttit;
	if (gconf_client_get_bool (rss_gconf, GCONF_KEY_STATUS_ICON, NULL)) {
		tchn = g_markup_escape_text (channel, -1);
		ttit = g_markup_escape_text (title, -1);
		total = g_strdup_printf("<b>%s</b>\n%s\n", tchn, ttit);
		g_print("total:%s\n", total);
		g_free(tchn);
		g_free(ttit);
		create_status_icon();
		iconfile = g_build_filename (EVOLUTION_ICONDIR,
			"rss-icon-unread.png",
			NULL);
		gtk_status_icon_set_from_file (
			status_icon,
			iconfile);
		g_free(iconfile);
		g_queue_push_tail(status_msg, total);
		if (g_queue_get_length(status_msg) == 6)
			g_queue_pop_head(status_msg);
		g_queue_foreach(status_msg, flaten_status, flat_status_msg);
#if GTK_CHECK_VERSION (2,16,0)
		gtk_status_icon_set_tooltip_markup (status_icon, flat_status_msg);
#else
		gtk_status_icon_set_tooltip (status_icon, flat_status_msg);
#endif
#if GTK_MINOR_VERSION < 22
		if (gconf_client_get_bool (rss_gconf, GCONF_KEY_BLINK_ICON, NULL)
		&& !gtk_status_icon_get_blinking(status_icon))
			gtk_status_icon_set_blinking (status_icon, TRUE);
		g_timeout_add(15 * 1000, flicker_stop, NULL);
#endif
		gtk_status_icon_set_has_tooltip (status_icon, TRUE);
		g_object_set_data_full (
			G_OBJECT (status_icon), "uri",
			g_strdup (lookup_feed_folder((gchar *)channel)),
			(GDestroyNotify) g_free);
		g_free(flat_status_msg);
//		g_free(total);
		flat_status_msg = NULL;
	}
}

