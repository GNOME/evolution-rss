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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "rss.h"
#include "rss-status-icon.h"

GtkStatusIcon *status_icon = NULL;
gboolean winstatus;
extern GQueue *status_msg;

void status_text_free(StatusText *st);

void
status_text_free(StatusText *st)
{
	g_free(st->chn_name);
	g_free(st->title);
	g_free(st);
}

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
		gchar *folder_name = lookup_feed_folder(uri);
		real_name = g_build_path(G_DIR_SEPARATOR_S,
				lookup_main_folder(),
				folder_name, NULL);
		g_free(folder_name);
		rss_select_folder(real_name);
	}
	g_queue_foreach(status_msg, (GFunc)status_text_free, NULL);
	status_msg = g_queue_new();
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
		g_signal_connect (
			G_OBJECT (status_icon),
			"activate",
			G_CALLBACK (icon_activated),
			NULL);
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
#if GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 22
	gtk_status_icon_set_blinking (status_icon, FALSE);
#endif
	return FALSE;
}

void
flatten_status(StatusText *st, gchar **user_data)
{
	gchar *temp = NULL;
	if (strlen(st->chn_name)) {
		gchar *total;
		gchar *tchn, *ttit;
		tchn = g_markup_escape_text (st->chn_name, -1);
		ttit = g_markup_escape_text (st->title, -1);
		total = g_strdup_printf("<b>%s</b>\n%s\n", tchn, ttit);
		g_free(tchn);
		g_free(ttit);
		if (*user_data)
			temp = g_strconcat(
						*user_data,
						total,
						NULL);
		else
			temp = g_strdup(total);
	}
	*user_data = temp;
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
	GtkWidget *evo_window = NULL;
	GList *windows, *link;

	windows = gtk_application_get_windows (GTK_APPLICATION (e_shell_get_default ()));
	for (link = windows; link; link = g_list_next (link)) {
		if (E_IS_SHELL_WINDOW (link->data)) {
			EShellWindow *shell_window = link->data;
			EShellView *shell_view;

			shell_view = e_shell_window_peek_shell_view (shell_window, "mail");
			if (shell_view) {
				evo_window = GTK_WIDGET (shell_window);
				if (g_strcmp0 (e_shell_window_get_active_view (shell_window), "mail") == 0)
					break;
			}
		}
	}

	if (!evo_window)
		return;

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
update_status_icon(GQueue *status)
{
	gchar *iconfile;
	StatusText *channel;
	gchar *flat = NULL;
	if (g_queue_is_empty(status))
		return;
	create_status_icon();
	iconfile = g_build_filename (EVOLUTION_ICONDIR,
			"rss-icon-unread.png",
			NULL);
	gtk_status_icon_set_from_file (
		status_icon,
		iconfile);
	g_free(iconfile);
	channel = g_queue_peek_tail(status);
	g_queue_foreach(status, (GFunc)flatten_status, &flat);
	if (flat)
#if GTK_CHECK_VERSION (2,16,0)
		gtk_status_icon_set_tooltip_markup (status_icon, flat);
#else
		gtk_status_icon_set_tooltip (status_icon, flat);
#endif
#if GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 22
	if (gconf_client_get_bool (client, GCONF_KEY_BLINK_ICON, NULL)
	&& !gtk_status_icon_get_blinking(status_icon))
		gtk_status_icon_set_blinking (status_icon, TRUE);
	g_timeout_add(15 * 1000, flicker_stop, NULL);
#endif
	gtk_status_icon_set_has_tooltip (status_icon, TRUE);
	g_object_set_data_full (
		G_OBJECT (status_icon), "uri",
		lookup_feed_folder((gchar *)channel->chn_name),
		(GDestroyNotify) g_free);
	g_free(flat);
}

void
update_status_icon_text(GQueue *status, const char *channel, gchar *title)
{
	StatusText *st = g_new0 (StatusText, 1);
	st->chn_name = g_strdup(channel);
	st->title = g_strdup(title);
	g_queue_push_tail(status, st);
	if (g_queue_get_length(status) == 6) {
		StatusText *tmp = g_queue_peek_head(status);
		g_free(tmp->chn_name);
		g_free(tmp->title);
		g_free(tmp);
		g_queue_pop_head(status);
	}
}

