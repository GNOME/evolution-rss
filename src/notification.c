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
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <mail/em-utils.h>

#include <shell/e-shell-taskbar.h>
#include <shell/e-shell-view.h>


extern int rss_verbose_debug;

#include "rss.h"
#include "network-soup.h"
#include "notification.h"

extern rssfeed *rf;

#if (EVOLUTION_VERSION < 29102)
static void
dialog_key_destroy (GtkWidget *widget, gpointer data)
{
	if (data)
		g_hash_table_remove(rf->error_hash, data);
}
#endif

void
err_destroy (GtkWidget *widget, guint response, gpointer data)
{
	gtk_widget_destroy(widget);
	rf->errdialog = NULL;
}

void
rss_error(gpointer key, gchar *name, gchar *error, gchar *emsg)
{
	GtkWidget *ed = NULL;
	gchar *msg;
#if (EVOLUTION_VERSION < 29102)
	gpointer newkey;
#endif
	EShell *shell;
	EMailBackend *backend;
	GtkWindow *parent;
	GList *windows;
#if (EVOLUTION_VERSION >= 30101)
	gpointer alert;
#endif
#if (EVOLUTION_VERSION >= 30101)
	GtkApplication *application;
#endif

	if (name)
		msg = g_strdup_printf("\n%s\n%s", name, emsg);
	else
		msg = g_strdup(emsg);

	if (key) {
		if (!g_hash_table_lookup(rf->error_hash, key)) {
			shell = e_shell_get_default ();
#if (EVOLUTION_VERSION >= 30101)
			alert = e_alert_new ("org-gnome-evolution-rss:feederr",
						error, msg, NULL);
			e_shell_submit_alert (shell, alert);
			goto out;
#endif
#if (EVOLUTION_VERSION >= 30301)
			application = GTK_APPLICATION(shell);
			windows = gtk_application_get_windows (application);
#else
			windows = e_shell_get_watched_windows (shell);
#endif
			parent = (windows != NULL) ? GTK_WINDOW (windows->data) : NULL;


#if (EVOLUTION_VERSION >= 29102)
			backend = (EMailBackend *)e_shell_get_backend_by_name (shell, "mail");
#if (EVOLUTION_VERSION >= 30303)
			e_alert_submit (
				e_mail_backend_get_alert_sink (backend),
				"org-gnome-evolution-rss:feederr",
				error, msg, NULL);
#else
			e_mail_backend_submit_alert (
				backend, "org-gnome-evolution-rss:feederr",
				error, msg, NULL);
#endif
#else
			ed = e_alert_dialog_new_for_args(parent,
				"org-gnome-evolution-rss:feederr",
				error, msg, NULL);
#endif
#if (EVOLUTION_VERSION < 29102)
			newkey = g_strdup(key);
			g_signal_connect(
				ed, "response",
				G_CALLBACK(err_destroy),
				NULL);
			g_object_set_data (
				(GObject *)ed, "response-handled",
				GINT_TO_POINTER (TRUE));
			g_signal_connect(ed,
				"destroy",
				G_CALLBACK(dialog_key_destroy),
				newkey);
			//lame widget destruction, seems e_activity timeout does not destroy it
			g_timeout_add_seconds(60,
				(GSourceFunc)gtk_widget_destroy,
				ed);
#endif

#if (EVOLUTION_VERSION < 29102)
		em_utils_show_error_silent(ed);
		g_hash_table_insert(
			rf->error_hash,
			newkey,
			GINT_TO_POINTER(1));
#endif
		}
		goto out;
	}

	if (!rf->errdialog) {
		shell = e_shell_get_default ();
#if (EVOLUTION_VERSION >= 30301)
		application = GTK_APPLICATION(shell);
		windows = gtk_application_get_windows (application);
#else
		windows = e_shell_get_watched_windows (shell);
#endif
		parent = (windows != NULL) ? GTK_WINDOW (windows->data) : NULL;

		ed  = e_alert_dialog_new_for_args(parent,
			"org-gnome-evolution-rss:feederr",
			error, msg, NULL);
		g_signal_connect(
			ed,
			"response",
			G_CALLBACK(err_destroy),
			NULL);
		gtk_widget_show(ed);
		rf->errdialog = ed;
	}

out:    g_free(msg);
}


void
taskbar_push_message (const gchar *message)
{
#if EVOLUTION_VERSION >= 22900
	EShellView *shell_view;
	EShellTaskbar *shell_taskbar;

	shell_view = rss_get_mail_shell_view (FALSE);

	g_return_if_fail (shell_view != NULL);

	shell_taskbar = e_shell_view_get_shell_taskbar (shell_view);
	e_shell_taskbar_set_message (shell_taskbar, message);
#endif
}

void
taskbar_pop_message(void)
{
	taskbar_push_message ("");
}

void
taskbar_op_abort(CamelOperation *cancellable, gpointer key)
{
	EActivity *activity = g_hash_table_lookup(rf->activity, key);
	e_activity_set_state (activity, E_ACTIVITY_CANCELLED);
	g_hash_table_remove(rf->activity, key);
	g_object_unref(activity);
	abort_all_soup();
}

EActivity *
taskbar_op_new(gchar *message, gpointer key);

EActivity *
taskbar_op_new(gchar *message, gpointer key)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EActivity *activity;
	GCancellable *cancellable;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

#if EVOLUTION_VERSION >= 23300
	activity = e_activity_new ();
#if (EVOLUTION_VERSION >= 29102)
	e_activity_set_text (activity, message);
#else
	e_activity_set_primary_text (activity, message);
#endif
	cancellable = camel_operation_new ();
	e_activity_set_cancellable (activity, cancellable);
#else
	activity = e_activity_new (message);
	e_activity_set_allow_cancel (activity, TRUE);
#endif
	e_activity_set_percent (activity, 0.0);
	e_shell_backend_add_activity (shell_backend, activity);

	g_signal_connect (
		cancellable, "cancelled",
		G_CALLBACK (taskbar_op_abort),
		key);
	g_object_unref (cancellable);
	return activity;
}

void
taskbar_op_set_progress(gchar *key, gchar *msg, gdouble progress)
{
	EActivity *activity_id;

	g_return_if_fail(key != NULL);

	activity_id = g_hash_table_lookup(rf->activity, key);

	if (activity_id) {
		e_activity_set_percent (activity_id, progress);
	}
}

void
taskbar_op_finish(gchar *key)
{
	EActivity *aid = NULL;
	EActivity *activity_key;
	if (key) {
		aid = (EActivity *)g_hash_table_lookup(rf->activity, key);
	}
	if (aid == NULL) {
		activity_key = g_hash_table_lookup(rf->activity, "main");
		if (activity_key) {
			d("activity_key:%p\n", (gpointer)activity_key);
#if (EVOLUTION_VERSION >= 29102)
			e_activity_set_state (activity_key, E_ACTIVITY_COMPLETED);
			g_object_unref(activity_key);
#else
			e_activity_complete (activity_key);
#endif
			g_hash_table_remove(rf->activity, "main");
		}
	} else {
#if (EVOLUTION_VERSION >= 29102)
		e_activity_set_state (aid, E_ACTIVITY_COMPLETED);
		g_object_unref(aid);
#else
		e_activity_complete (aid);
#endif
		g_hash_table_remove(rf->activity, key);
	}
}

EActivity*
taskbar_op_message(gchar *msg, gchar *unikey)
{
		gchar *tmsg;
		EActivity *activity_id;
		if (!msg) {
			tmsg = g_strdup_printf(
				_("Fetching Feeds (%d enabled)"),
				g_hash_table_size(rf->hrname));
			unikey = (gchar *)"main";
		} else
			tmsg = g_strdup(msg);

		if (!msg)
			activity_id =
				(EActivity *)taskbar_op_new(
					tmsg,
					unikey);
		else
			activity_id =
				(EActivity *)taskbar_op_new(tmsg, msg);
		g_hash_table_insert(
			rf->activity,
			GUINT_TO_POINTER(unikey),
			GUINT_TO_POINTER(activity_id));
		g_free(tmsg);
		return activity_id;
}

