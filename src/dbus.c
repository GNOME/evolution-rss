/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2009  Lucian Langa <cooly@gnome.eu.org>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

extern int rss_verbose_debug;

#include "rss.h"
#include "rss-config-factory.h"
#include "misc.h"
#include "notification.h"
#include "dbus.h"

#define DBUS_PATH "/org/gnome/evolution/mail/rss"
#define RSS_DBUS_SERVICE "org.gnome.evolution.mail.rss"
//#define DBUS_INTERFACE "org.gnome.evolution.mail.rss.in"
#define DBUS_INTERFACE "org.gnome.evolution.mail.rss"
#define DBUS_REPLY_INTERFACE "org.gnome.evolution.mail.rss.out"

static GDBusConnection *connection = NULL;
static gboolean enabled = FALSE;

extern rssfeed *rf;

static gboolean
reinit_dbus (gpointer user_data)
{
	if (!enabled || init_gdbus ())
		return FALSE;

	/* keep trying to re-establish dbus connection */

	return TRUE;
}

static void
connection_closed_cb (GDBusConnection *pconnection,
	gboolean remote_peer_vanished, GError *error,
	gpointer user_data)
{
	g_return_if_fail (connection != pconnection);
	g_object_unref (connection);
	connection = NULL;

	g_timeout_add (3000, reinit_dbus, NULL);
}

static void
signal_cb (GDBusConnection *connection,
		const gchar *sender_name,
		const gchar *object_path,
		const gchar *interface_name,
		const gchar *signal_name,
		GVariant *parameters,
		gpointer user_data)
{
	if (g_strcmp0 (interface_name, DBUS_INTERFACE) != 0
	|| g_strcmp0 (object_path, DBUS_PATH) != 0)
		return;

	d("interface:%s!\n", interface_name);
	d("path:%s!\n", object_path);
	d("signal_name:%s!\n", signal_name);


	if (!g_strcmp0 (signal_name, "import")) {
		gchar *url = NULL;
		add_feed *feed = g_new0(add_feed, 1);
		g_variant_get (parameters, "(s)", &url);
		feed->feed_url = url;
		feed->add=1;
		feed->enabled=feed->validate=1;
		feed->fetch_html = 0;
		if (feed->feed_url && strlen(feed->feed_url)) {
			g_print("New Feed received: %s\n", url);
			feed->feed_url = sanitize_url(feed->feed_url);
			d("sanitized feed URL: %s\n", feed->feed_url);
			if (g_hash_table_find(rf->hr, check_if_match, feed->feed_url)) {
				rss_error(NULL, NULL, _("Error adding feed."),
					_("Feed already exists!"));
					return;
			}
			if (setup_feed(feed)) {
				gchar *msg = g_strdup_printf(_("Importing URL: %s"),
						feed->feed_url);
				taskbar_push_message(msg);
				g_free(msg);
			}
			if (rf->treeview)
				store_redraw(GTK_TREE_VIEW(rf->treeview));
			save_gconf_feed();
#if (DATASERVER_VERSION >= 2033001)
			camel_operation_pop_message (NULL);
#else
		camel_operation_end(NULL);
#endif
		}
		g_free(url);
	}

	if (!g_strcmp0 (signal_name, "ping")) {
		GDBusMessage *message;
		GVariantBuilder *builder;
		GError *error = NULL;
		d("Ping! received from %s\n", interface_name);

		if (!(message = g_dbus_message_new_signal (DBUS_PATH, DBUS_INTERFACE, "pong")))
			return;
		d("Sending Pong! back\n");
		builder = g_variant_builder_new (G_VARIANT_TYPE_TUPLE);
		g_variant_builder_add(builder, "s", "ponging");
		g_dbus_message_set_body (message, g_variant_builder_end (builder));
		g_dbus_connection_send_message (connection, message,
			G_DBUS_SEND_MESSAGE_FLAGS_NONE, NULL, &error);
		g_object_unref (message);
		if (error) {
			g_debug ("Error while sending ping-request: %s", error->message);
			g_error_free (error);
		}
	}
}

gboolean
init_gdbus (void)
{
	GError *error = NULL;

	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (error) {
		g_warning ("could not get system bus: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	g_dbus_connection_set_exit_on_close (connection, FALSE);
	g_signal_connect (connection, "closed",
		G_CALLBACK (connection_closed_cb), NULL);

	if (!g_dbus_connection_signal_subscribe (
		connection,
		NULL,
		RSS_DBUS_SERVICE,
/*		DBUS_INTERFACE,*/
		NULL,
		DBUS_PATH,
		NULL,
		G_DBUS_SIGNAL_FLAGS_NONE,
		signal_cb,
		NULL,
		NULL)) {
			g_warning ("%s: Failed to subscribe for a signal", G_STRFUNC);
			goto fail;
		}
	return FALSE;


fail:	g_object_unref(connection);
	return TRUE;
}

