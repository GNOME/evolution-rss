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
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
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

#define DBUS_PATH "/org/gnome/evolution/mail/rss"
#define RSS_DBUS_SERVICE "org.gnome.evolution.mail.rss"
//#define DBUS_INTERFACE "org.gnome.evolution.mail.rss.in"
#define DBUS_INTERFACE "org.gnome.evolution.mail.rss"
#define DBUS_REPLY_INTERFACE "org.gnome.evolution.mail.rss.out"

//evolution ping roud-trip time in ms, somebody suggest a real value here
#define EVOLUTION_PING_TIMEOUT 5000

static GDBusConnection *connection = NULL;

static gboolean init_gdbus (void);

static gboolean enabled = FALSE;
GMainLoop *loop;
gboolean evo_running = FALSE;
static gchar *feed = NULL;


static void
send_dbus_ping (void)
{
	GDBusMessage *message;
	GVariantBuilder *builder;
	GError *error = NULL;

	if (!(message = g_dbus_message_new_signal (DBUS_PATH, DBUS_INTERFACE, "ping")))
		return;
	printf("ping evolution...\n");
	builder = g_variant_builder_new (G_VARIANT_TYPE_TUPLE);
	g_variant_builder_add(builder, "s", "evolution-rss");
	g_dbus_message_set_body (message, g_variant_builder_end (builder));
	g_dbus_connection_send_message (connection, message,
		G_DBUS_SEND_MESSAGE_FLAGS_NONE, NULL, &error);
	g_object_unref (message);
	if (error) {
		g_debug ("Error while sending ping-request: %s", error->message);
		g_error_free (error);
	}
}

static void
send_dbus_message (const char *name, const char *data)
{
	GDBusMessage *message;
	GVariantBuilder *builder;
	GError *error = NULL;

	/* Create a new message on the DBUS_INTERFACE */
	if (!(message = g_dbus_message_new_signal (DBUS_PATH, DBUS_INTERFACE, name)))
		return;

	builder = g_variant_builder_new (G_VARIANT_TYPE_TUPLE);

	/* Appends the data as an argument to the message */
	g_variant_builder_add (builder, "s", data);
	g_dbus_message_set_body (message, g_variant_builder_end (builder));

	/* Sends the message */
	g_dbus_connection_send_message (connection, message, G_DBUS_SEND_MESSAGE_FLAGS_NONE, NULL, &error);

	/* Frees the message */
	g_object_unref (message);

	if (error) {
		g_debug ("%s: Error while sending DBus message: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
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

	d(g_print("interface:%s!\n", interface_name));
	d(g_print("path:%s!\n", object_path));
	d(g_print("signal_name:%s!\n", signal_name));

	if (!g_strcmp0 (signal_name, "pong")) {
		evo_running = TRUE;
		d(g_print("pong\n"));
		send_dbus_message ("import", feed);
		g_usleep(300);
		g_main_loop_quit(loop);
	}
}

static void
connection_closed_cb  (GDBusConnection *pconnection,
	gboolean remote_peer_vanished, GError *error, gpointer user_data)
{
	g_return_if_fail (connection != pconnection);
	g_object_unref (connection);
	connection = NULL;

	g_timeout_add (3000, reinit_gdbus, NULL);
#if 0
	else if (dbus_message_is_signal (message, DBUS_REPLY_INTERFACE, "pong")) {
		g_print("pong!\n");
		evo_running = TRUE;
		g_main_loop_quit(loop);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
#endif
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
no_evo_cb (gpointer user_data)
{
	if (!evo_running) {
		g_print("no evolution running!\n");
		g_print("trying to start...\n");
		system("evolution&");
		g_usleep(30);
		send_dbus_ping ();
		return FALSE;
	}
}


static gboolean
main_prog(gpointer user_data)
{
	if (connection != NULL)
		send_dbus_ping ();
	g_timeout_add (EVOLUTION_PING_TIMEOUT, no_evo_cb, NULL);
	return FALSE;
}

static gboolean
err_evo_cb (gpointer user_data)
{
	g_print("cannot start evolution...retry %d\n", GPOINTER_TO_INT(user_data));
	g_main_loop_quit(loop);
	return TRUE;
}

static void
on_bus_acquired (GDBusConnection *connection,
		const gchar     *name,
		gpointer         user_data)
{
	g_print("Auquired bus connection.\n");
}

static void
on_name_acquired (GDBusConnection *connection,
		const gchar     *name,
		gpointer         user_data)
{
	g_print("Name aquired.\n");
}

static void
on_name_lost (GDBusConnection *connection,
		const gchar     *name,
		gpointer         user_data)
{
	g_print("Name lost.\n");
}

int
main (int argc, char *argv[])
{
	guint i=0;
	guint owner_id;

	feed = argv[1];
	if (!feed) {
		g_print("Syntax: %s URL\n", argv[0]);
		return 0;
	}

	g_type_init ();


	owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
		RSS_DBUS_SERVICE,
		G_BUS_NAME_OWNER_FLAGS_NONE,
		on_bus_acquired,
		on_name_acquired,
		on_name_lost,
		NULL,
		NULL);

	loop = g_main_loop_new (NULL, FALSE);

	if (!init_gdbus ())
		return -1;

	g_signal_connect (connection, "closed",
		G_CALLBACK (connection_closed_cb), NULL);

	g_timeout_add (100, main_prog, NULL);

	if (!g_dbus_connection_signal_subscribe (
		connection,
		NULL,
		RSS_DBUS_SERVICE,
		NULL,
		NULL,
		NULL,
		G_DBUS_SIGNAL_FLAGS_NONE,
		signal_cb,
		NULL,
		NULL)) {
			g_warning ("%s: Failed to subscribe for a signal", G_STRFUNC);
			goto fail;
		}

	g_main_loop_run(loop);

	while (!evo_running && i < 2) {
		system("evolution&");
		g_print("fireing evolution...\n");
		g_usleep(30);
		send_dbus_ping ();
		g_timeout_add (EVOLUTION_PING_TIMEOUT, err_evo_cb, GINT_TO_POINTER(i++));
		g_main_loop_run(loop);
	}

#if 0
	if (evo_running) {
		if (s)
			send_dbus_message ("evolution_rss_feed", s);
		else
			g_print("Syntax: evolution-import-rss URL\n");
	} else {
		g_print("evolution repetably failed to start!\n");
		g_print("Cannot add feed!");
	}

	if (bus != NULL) {
		dbus_connection_unref (bus);
		bus = NULL;
	}
	return 0;
#endif

fail:	return FALSE;

}

