/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2011  Lucian Langa <cooly@gnome.eu.org>
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

#define RSS_DBUS_PATH "/org/gnome/feed/Reader"
#define RSS_DBUS_SERVICE "org.gnome.feed.Reader"
#define RSS_DBUS_INTERFACE "org.gnome.feed.Reader"

gboolean subscribe_method(gchar *url);

static void
method_call_cb (GDBusConnection       *connection,
		const gchar           *sender,
		const gchar           *object_path,
		const gchar           *interface_name,
		const gchar           *method_name,
		GVariant              *parameters,
		GDBusMethodInvocation *invocation,
		gpointer               user_data);

static GDBusConnection *connection = NULL;

static GDBusNodeInfo *nodeinfo = NULL;

static const gchar introspection_xml[] =
	"<node name='/org/gnome/feed/Reader'>"
		"<interface name='org.gnome.feed.Reader'>"
			"<method name='Ping'>"
				"<arg name='result' type='b' direction='out'/>"
			"</method>"
			"<method name='Subscribe'>"
				"<arg name='url' type='s'/>"
				"<arg name='result' type='b' direction='out'/>"
			"</method>"
		"</interface>"
	"</node>";

static const GDBusInterfaceVTable vtable =
{
	method_call_cb,
	NULL,
	NULL
};

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

gboolean
subscribe_method(gchar *url)
{
	add_feed *feed = g_new0(add_feed, 1);
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
				//return FALSE;
				/* we return true here since org.gnome.feed.Reader
				 * doesn't support status */
				return TRUE;
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
method_call_cb (GDBusConnection       *connection,
		const gchar           *sender,
		const gchar           *object_path,
		const gchar           *interface_name,
		const gchar           *method_name,
		GVariant              *parameters,
		GDBusMethodInvocation *invocation,
		gpointer               user_data)
{
	gchar *url;
	gboolean res = FALSE;
	d("method:%s\n", method_name);
	if (!g_strcmp0 (method_name, "Subscribe")) {
		g_variant_get (parameters, "(s)", &url);
		res = subscribe_method(url);
		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(b)", res));
	}
	if (!g_strcmp0 (method_name, "Ping")) {
		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(b)", TRUE));
	}
}

static void
on_bus_acquired (GDBusConnection *connection,
		const gchar     *name,
		gpointer         user_data)
{
	guint reg_id;
	GError     *error = NULL;
	nodeinfo = g_dbus_node_info_new_for_xml (introspection_xml,
				NULL);

	reg_id = g_dbus_connection_register_object (connection,
		RSS_DBUS_PATH,
		nodeinfo->interfaces[0],
		&vtable,
		NULL, NULL,
		&error);
	if (!reg_id) {
		g_printerr ("Failed to register bus object: %s\n", error->message);
		g_error_free (error);
	}
}

static void
on_name_acquired (GDBusConnection *connection,
		const gchar     *name,
		gpointer         user_data)
{
	d("Name aquired.\n");
}

static void
on_name_lost (GDBusConnection *connection,
		const gchar     *name,
		gpointer         user_data)
{
	d("Name lost.\n");
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

	g_bus_own_name (G_BUS_TYPE_SESSION,
		RSS_DBUS_SERVICE,
		G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
		on_bus_acquired,
		on_name_acquired,
		on_name_lost,
		NULL,
		NULL);

	return FALSE;
}

