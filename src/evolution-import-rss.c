/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2010 Lucian Langa <cooly@gnome.eu.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <glib.h>

#include <gio/gio.h>

#define d(x)
#define EVOLUTION "/opt/gnome-dev/bin/evolution&"

#define RSS_DBUS_PATH "/org/gnome/feed/Reader"
#define RSS_DBUS_SERVICE "org.gnome.feed.Reader"
#define RSS_DBUS_INTERFACE "org.gnome.feed.Reader"

//evolution ping roud-trip time in ms, somebody suggest a real value here
#define EVOLUTION_PING_TIMEOUT 5000

#define run(x) G_STMT_START { g_message ("%s", x); system (x); } G_STMT_END

static GDBusConnection *connection = NULL;
GDBusProxy *proxy;

static gboolean init_gdbus (void);

static gboolean enabled = FALSE;
GMainLoop *loop;
gboolean evo_running = FALSE;
const gchar *feed = NULL;

gboolean send_dbus_ping (void);
gboolean subscribe_feed(const gchar *url);

gboolean
send_dbus_ping (void)
{
	GVariant *ret;
	gboolean val = FALSE;

	d("ping evolution...\n");
	ret = g_dbus_proxy_call_sync (proxy, "Ping",
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL);
	if (ret)
		g_variant_get_child (ret, 0, "b", &val);
	return val;
}

gboolean
subscribe_feed(const gchar *url)
{
	GVariant *ret;
	gboolean val;

	ret = g_dbus_proxy_call_sync (proxy, "Subscribe",
			g_variant_new("(s)", url),
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL);
	g_variant_get_child (ret, 0, "b", &val);
	return val;
}

static gboolean
reinit_gdbus (gpointer user_data)
{
	if (!enabled || init_gdbus ())
		return FALSE;

	/* keep trying to re-establish dbus connection */

	return TRUE;
}

static void
connection_closed_cb  (GDBusConnection *pconnection,
	gboolean remote_peer_vanished, GError *error, gpointer user_data)
{
	g_return_if_fail (connection != pconnection);
	g_object_unref (connection);
	connection = NULL;

	g_timeout_add (3000, reinit_gdbus, NULL);
}

static gboolean
init_gdbus (void)
{
	GError *error = NULL;

	if (connection != NULL)
		return TRUE;
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (error) {
		g_warning ("could not get system bus: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}
	g_dbus_connection_set_exit_on_close (connection, FALSE);
	g_signal_connect (connection, "closed", G_CALLBACK (connection_closed_cb), NULL);

	return TRUE;
}

static gboolean
err_evo_cb (gpointer user_data)
{
	g_print("cannot start evolution...retry %d\n", GPOINTER_TO_INT(user_data));
	g_main_loop_quit(loop);
	return TRUE;
}

int
main (int argc, char *argv[])
{
	guint i=0;

	feed = argv[1];
	if (!feed) {
		g_print("Syntax: %s URL\n", argv[0]);
		feed = "";
	}

	g_type_init ();


	loop = g_main_loop_new (NULL, FALSE);

	if (!init_gdbus ())
		return -1;

	g_signal_connect (connection, "closed",
		G_CALLBACK (connection_closed_cb), NULL);

	proxy = g_dbus_proxy_new_sync (connection,
			G_DBUS_PROXY_FLAGS_NONE,
			NULL,
			RSS_DBUS_SERVICE,
			RSS_DBUS_PATH,
			RSS_DBUS_INTERFACE,
			NULL,
			NULL);

	evo_running = send_dbus_ping ();

	while (!evo_running && i<2) {
		run(EVOLUTION);
		g_print("Starting evolution...\n");
		while (!(evo_running = send_dbus_ping ()))
			sleep(1);
		if (evo_running)
			break;
		g_timeout_add (EVOLUTION_PING_TIMEOUT, err_evo_cb, GINT_TO_POINTER(i++));
		g_main_loop_run(loop);
	}

	if (evo_running) {
		gboolean result = subscribe_feed(feed);
		g_print("Success:%d\n", result);
		return result;
	}

	return FALSE;
}

