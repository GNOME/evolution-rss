/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Author: Miguel Angel Lopez Hernandez <miguel@gulev.org.mx>
 *
 *  Copyright 2004 Novell, Inc.
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#define DBUS_PATH "/org/gnome/evolution/mail/rss"
#define DBUS_INTERFACE "org.gnome.evolution.mail.rss.in"
#define DBUS_REPLY_INTERFACE "org.gnome.evolution.mail.rss.out"

static gboolean init_dbus (void);

static DBusConnection *bus = NULL;
static gboolean enabled = FALSE;
GMainLoop *loop;
gboolean evo_running = FALSE;

static void
send_dbus_ping (void)
{
	DBusMessage *message;
	DBusPendingCall *pending;
	if (!(message = dbus_message_new_signal (DBUS_PATH, DBUS_INTERFACE, "ping")))
		return;
	int ret = dbus_connection_send (bus, message, NULL);
	if (ret == FALSE)
    	{
     		printf("Could not send method call\n");
	}
	dbus_message_unref (message);
}

static void
send_dbus_message (const char *name, const char *data)
{
	DBusMessage *message;
	int serial= 123;
	
	/* Create a new message on the DBUS_INTERFACE */
	if (!(message = dbus_message_new_signal (DBUS_PATH, DBUS_INTERFACE, name)))
		return;
	
	/* Appends the data as an argument to the message */
	dbus_message_append_args (message,
//#if DBUS_VERSION >= 310
			  DBUS_TYPE_STRING, &data,
//#else
//			  DBUS_TYPE_STRING, data,
//#endif
			  DBUS_TYPE_INVALID);

	/* Sends the message */
	dbus_connection_send (bus, message, NULL);
	
	/* Frees the message */
	dbus_message_unref (message);
}

static gboolean
reinit_dbus (gpointer user_data)
{
	if (!enabled || init_dbus ())
		return FALSE;
	
	/* keep trying to re-establish dbus connection */
	
	return TRUE;
}

static DBusHandlerResult
filter_function (DBusConnection *connection, DBusMessage *message, void *user_data)
{
	if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected") &&
	    strcmp (dbus_message_get_path (message), DBUS_PATH_LOCAL) == 0) {
		dbus_connection_unref (bus);
		bus = NULL;
		
		g_timeout_add (3000, reinit_dbus, NULL);
		
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	else if (dbus_message_is_signal (message, DBUS_REPLY_INTERFACE, "pong")) {
		g_print("pong!\n");
		evo_running = TRUE;
		g_main_loop_quit(loop);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static gboolean
init_dbus (void)
{
	DBusError error;
	
	if (bus != NULL)
		return TRUE;
	
	dbus_error_init (&error);
	if (!(bus = dbus_bus_get (DBUS_BUS_SESSION, &error))) {
		g_warning ("could not get system bus: %s\n", error.message);
		dbus_error_free (&error);
		return FALSE;
	}
	
	dbus_connection_setup_with_g_main (bus, NULL);
	dbus_bus_add_match (bus, "type='signal'", NULL);
	dbus_connection_set_exit_on_disconnect (bus, FALSE);
	dbus_connection_add_filter (bus, filter_function, loop, NULL);
	
	return TRUE;
}

static gboolean
no_evo_cb (gpointer user_data)
{
	g_print("no evolution running!\n");
	g_print("trying to start...\n");
	g_main_loop_quit(loop);
}

static gboolean
err_evo_cb (gpointer user_data)
{
	g_print("cannot start evolution...retry %d\n", user_data);
	g_main_loop_quit(loop);
}

int
main (int argc, char *argv[])
{
	guint i=0;
	loop = g_main_loop_new (NULL, FALSE);

	if (!init_dbus ())
		return -1;

	char *s = argv[1];

	if (bus != NULL)
                send_dbus_ping ();
	g_timeout_add (2000, no_evo_cb, NULL);
	g_main_loop_run(loop);
	while (!evo_running && i < 3)
	{
		system("evoltion&");
		g_print("fireing evolution...\n");
		sleep(10);
        	send_dbus_ping ();
		g_timeout_add (2000, err_evo_cb, i++);
		g_main_loop_run(loop);
	}
	

	if (evo_running)
        	send_dbus_message ("evolution_rss_feed", s);
	else
	{
		g_print("evolution repetably failed to start!\n");
		g_print("Cannot add feed!");
	}
	
	if (bus != NULL) {
		dbus_connection_unref (bus);
		bus = NULL;
	}
	return 0;
}

