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

#include <string.h>
#include <glib.h>
//#include <gconf/gconf-client.h>
//include <e-util/e-config.h>
//#include <mail/em-utils.h>
//#include <mail/em-event.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
//#include <camel/camel-folder.h>

#include <rss.h>

#define DBUS_PATH "/org/gnome/evolution/mail/rss"
#define DBUS_INTERFACE "org.gnome.evolution.mail.dbus.Signal"

/*int e_plugin_lib_enable (EPluginLib *ep, int enable);
void org_gnome_new_mail_notify (EPlugin *ep, EMEventTargetFolder *t);
void org_gnome_message_reading_notify (EPlugin *ep, EMEventTargetMessage *t);*/

static DBusConnection *init_dbus (void);

static DBusConnection *bus = NULL;
static gboolean enabled = FALSE;

/*static void
send_dbus_message (const char *name, const char *data, guint new)
{
	DBusMessage *message;
	
	/* Create a new message on the DBUS_INTERFACE */
/*	if (!(message = dbus_message_new_signal (DBUS_PATH, DBUS_INTERFACE, name)))
		return;
	
	/* Appends the data as an argument to the message */
/*	dbus_message_append_args (message,
#if DBUS_VERSION >= 310
				  DBUS_TYPE_STRING, &data,
#else
				  DBUS_TYPE_STRING, data,
#endif	
				  DBUS_TYPE_INVALID);

	if (new) {
		char * display_name = em_utils_folder_name_from_uri(data);
		dbus_message_append_args (message,
#if DBUS_VERSION >= 310
					  DBUS_TYPE_STRING, &display_name, DBUS_TYPE_UINT32, &new,
#else
					  DBUS_TYPE_STRING, display_name, DBUS_TYPE_UINT32, new,
#endif	
					  DBUS_TYPE_INVALID);
		
	}

	/* Sends the message */
//	dbus_connection_send (bus, message, NULL);
	
	/* Frees the message */
/*	dbus_message_unref (message);
}*/

/*void
org_gnome_message_reading_notify (EPlugin *ep, EMEventTargetMessage *t)
{
	if (bus != NULL)
		send_dbus_message ("MessageReading", t->folder->name, 0);
}

void
org_gnome_new_mail_notify (EPlugin *ep, EMEventTargetFolder *t)
{
	if (bus != NULL)
		send_dbus_message ("Newmail", t->uri, t->new);
}*/


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
	else if (dbus_message_is_signal (message, DBUS_INTERFACE, "evolution_rss_feed")) {
		DBusError error;
    		char *s;
		add_feed *feed = g_new0(add_feed, 1);
    		dbus_error_init (&error);
    		if (dbus_message_get_args 
       			(message, &error, DBUS_TYPE_STRING, &s, DBUS_TYPE_INVALID)) {
      			g_print("New Feed received: %s\n", s);
			feed->feed_url = g_strdup(s);
			feed->add=1;
			feed->enabled=feed->validate=1;
			feed->fetch_html = 0;
			if (feed->feed_url && strlen(feed->feed_url))
        		{
                		gchar *text = feed->feed_url;
                		feed->feed_url = sanitize_url(feed->feed_url);
                		g_free(text);
                		if (g_hash_table_find(rf->hr,
                                        check_if_match,
                        	        feed->feed_url))
                		{
                        	   rss_error(NULL, _("Error adding feed."),
                        	                   _("Feed already exists!"));
                        	   exit;
                		}
                		setup_feed(feed);
        			save_gconf_feed();
			}

      			//dbus_free (s);
    		} else {
      			g_print("Feed received, but error getting message: %s\n", error.message);
      			dbus_error_free (&error);
    		}
    		return DBUS_HANDLER_RESULT_HANDLED;
  	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusConnection *
init_dbus (void)
{
	static DBusConnection *bus = NULL;
	DBusError error;
	GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);

	
	if (rf->bus != NULL)
		return rf->bus;
	
	dbus_error_init (&error);
	if (!(bus = dbus_bus_get (DBUS_BUS_SESSION, &error))) {
		g_warning ("could not get system bus: %s\n", error.message);
		dbus_error_free (&error);
		return NULL;
	}
	
	dbus_connection_setup_with_g_main (bus, NULL);
	  dbus_bus_add_match (bus, "type='signal',interface='org.gnome.evolution.mail.dbus.Signal'", NULL);
	dbus_connection_set_exit_on_disconnect (bus, FALSE);
	
	dbus_connection_add_filter (bus, filter_function, loop, NULL);
	
	return bus;
}


/*int
e_plugin_lib_enable (EPluginLib *ep, int enable)
{
	if (enable) {
		if (!init_dbus ())
			return -1;
		
		enabled = TRUE;
	} else {
		if (bus != NULL) {
			dbus_connection_unref (bus);
			bus = NULL;
		}
		
		enabled = FALSE;
	}
	
	return 0;
}*/

