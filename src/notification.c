/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2009 Lucian Langa <cooly@gnome.eu.org>
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

#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <mail/em-utils.h>
#if (EVOLUTION_VERSION >= 22900) //kb//
#include <e-util/e-alert-dialog.h>
#include <shell/e-shell-taskbar.h>
#include <shell/e-shell-view.h>
#else
#include <e-util/e-error.h>
#endif

extern int rss_verbose_debug;

#include "rss.h"
#include "network-soup.h"
#include "notification.h"

#if (EVOLUTION_VERSION >= 22900) //kb//
extern EShellView *rss_shell_view;
#endif
extern rssfeed *rf;

static void
dialog_key_destroy (GtkWidget *widget, gpointer data)
{
	if (data)
		g_hash_table_remove(rf->error_hash, data);
}

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
	gpointer newkey;
#if (EVOLUTION_VERSION >= 22900) //kb//
	EShell *shell;
	EMailBackend *backend;
	GtkWindow *parent;
	GList *windows;
#else
	EActivityHandler *activity_handler;
	guint id;
#endif

	if (name)
		msg = g_strdup_printf("\n%s\n%s", name, emsg);
	else
		msg = g_strdup(emsg);

	if (key) {
		if (!g_hash_table_lookup(rf->error_hash, key)) {
#if (EVOLUTION_VERSION >= 22900) //kb//
			shell = e_shell_get_default ();
			windows = e_shell_get_watched_windows (shell);
			parent = (windows != NULL) ? GTK_WINDOW (windows->data) : NULL;

#if (EVOLUTION_VERSION >= 29102)
			backend = (EMailBackend *)e_shell_get_backend_by_name (shell, "mail");
			e_mail_backend_submit_alert (
				backend, "org-gnome-evolution-rss:feederr",
				error, msg, NULL);
#else
			ed = e_alert_dialog_new_for_args(parent,
				"org-gnome-evolution-rss:feederr",
				error, msg, NULL);
#endif
#else
			ed = e_error_new(NULL, "org-gnome-evolution-rss:feederr",
				error, msg, NULL);
#endif
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

#if (EVOLUTION_VERSION < 29102)
#if (EVOLUTION_VERSION >= 22900) //kb//
		em_utils_show_error_silent(ed);
		g_hash_table_insert(
			rf->error_hash,
			newkey,
			GINT_TO_POINTER(1));

#else
		activity_handler =
			mail_component_peek_activity_handler (mail_component_peek());
		id = e_activity_handler_make_error (
			activity_handler,
			(char *)mail_component_peek(),
			E_LOG_ERROR,
			ed);
		g_hash_table_insert(rf->error_hash,
			newkey,
			GINT_TO_POINTER(id));
#endif
#endif
		}
		goto out;
	}

	if (!rf->errdialog) {
#if (EVOLUTION_VERSION >= 22900) //kb//
		shell = e_shell_get_default ();
		windows = e_shell_get_watched_windows (shell);
		parent = (windows != NULL) ? GTK_WINDOW (windows->data) : NULL;

		ed  = e_alert_dialog_new_for_args(parent,
			"org-gnome-evolution-rss:feederr",
			error, msg, NULL);
#else
		ed  = e_error_new(NULL,
			"org-gnome-evolution-rss:feederr",
			error, msg, NULL);
#endif
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
taskbar_push_message(gchar *message)
{
#if EVOLUTION_VERSION < 22900 //kb//
	EActivityHandler *activity_handler =
		mail_component_peek_activity_handler (mail_component_peek ());
	e_activity_handler_set_message(activity_handler, message);
#else
	EShellTaskbar *shell_taskbar;
	g_return_if_fail(rss_shell_view != NULL);
	shell_taskbar = e_shell_view_get_shell_taskbar (rss_shell_view);
	e_shell_taskbar_set_message (shell_taskbar, message);
#endif
}

void
taskbar_pop_message(void)
{
#if EVOLUTION_VERSION < 22900 //kb//
	EActivityHandler *activity_handler =
		mail_component_peek_activity_handler (mail_component_peek ());
	e_activity_handler_unset_message(activity_handler);
#else
	EShellTaskbar *shell_taskbar;
	g_return_if_fail(rss_shell_view != NULL);
	shell_taskbar = e_shell_view_get_shell_taskbar (rss_shell_view);
	e_shell_taskbar_set_message (shell_taskbar, "");
#endif
}

void
#if EVOLUTION_VERSION > 29102
taskbar_op_abort(CamelOperation *cancellable, gpointer key)
#else
taskbar_op_abort(gpointer key)
#endif
{
#if EVOLUTION_VERSION < 22900 //kb//
	EActivityHandler *activity_handler =
		mail_component_peek_activity_handler (mail_component_peek ());
	guint activity_key =
		GPOINTER_TO_INT(g_hash_table_lookup(rf->activity, key));
	if (activity_key)
		e_activity_handler_operation_finished(
				activity_handler,
				activity_key);
#else
	EActivity *activity = g_hash_table_lookup(rf->activity, key);
	e_activity_set_state (activity, E_ACTIVITY_CANCELLED);
	g_hash_table_remove(rf->activity, key);
	g_object_unref(activity);
	abort_all_soup();
#endif
}

#if EVOLUTION_VERSION >= 22900 //kb//
EActivity *
#else
guint
#endif
taskbar_op_new(gchar *message, gpointer key);

#if EVOLUTION_VERSION >= 22900 //kb//
EActivity *
#else
guint
#endif
taskbar_op_new(gchar *message, gpointer key)
{
#if EVOLUTION_VERSION >= 22900 //kb//
	EShell *shell;
	EShellBackend *shell_backend;
	EActivity *activity;
	GCancellable *cancellable;
#else
	EActivityHandler *activity_handler;
	char *mcp;
	guint activity_id;
#endif

#if EVOLUTION_VERSION >= 22900 //kb//

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
#else
	activity_handler =
		mail_component_peek_activity_handler (mail_component_peek ());
	mcp = g_strdup_printf("%p", mail_component_peek());
	activity_id =
		e_activity_handler_cancelable_operation_started(
			activity_handler,
			"evolution-mail",
			message,
			TRUE,
			(void (*) (gpointer))taskbar_op_abort,
			key);
	g_free(mcp);
	return activity_id;
#endif //kb//
}

void
taskbar_op_set_progress(gchar *key, gchar *msg, gdouble progress)
{
#if (EVOLUTION_VERSION < 22900) //kb//
	EActivityHandler *activity_handler;
	guint activity_id;
#else
	EActivity *activity_id;
#endif

	g_return_if_fail(key != NULL);

#if (EVOLUTION_VERSION < 22900) //kb//
	activity_handler = mail_component_peek_activity_handler
				(mail_component_peek ());
	activity_id = GPOINTER_TO_INT(
				g_hash_table_lookup(rf->activity, key));
#else
	activity_id = g_hash_table_lookup(rf->activity, key);
#endif

	if (activity_id) {
#if (EVOLUTION_VERSION < 22900) //kb//
		e_activity_handler_operation_progressing(
			activity_handler,
			activity_id,
			g_strdup(msg),
			progress);
#else
	e_activity_set_percent (activity_id, progress);
#endif
	}
}

void
taskbar_op_finish(gchar *key)
{
#if (EVOLUTION_VERSION >= 22900) //kb//
	EActivity *aid = NULL;
	EActivity *activity_key;
#else
	guint aid = 0;
	guint activity_key;
	EActivityHandler *activity_handler = mail_component_peek_activity_handler (
						mail_component_peek ());
#endif
	if (key) {
#if (EVOLUTION_VERSION >= 22900) //kb//
		aid = (EActivity *)g_hash_table_lookup(rf->activity, key);
#else
		aid = (guint)g_hash_table_lookup(rf->activity, key);
#endif
	}
	if (aid == NULL) {
#if (EVOLUTION_VERSION >= 22900) //kb//
		activity_key = g_hash_table_lookup(rf->activity, "main");
#else
		activity_key = GPOINTER_TO_INT(g_hash_table_lookup(rf->activity, "main"));
#endif
		if (activity_key) {
			d("activity_key:%p\n", (gpointer)activity_key);
#if (EVOLUTION_VERSION >= 22900) //kb//
#if (EVOLUTION_VERSION >= 29102)
			e_activity_set_state (activity_key, E_ACTIVITY_COMPLETED);
			g_object_unref(activity_key);
#else
			e_activity_complete (activity_key);
#endif
#else
			e_activity_handler_operation_finished(activity_handler, activity_key);
#endif
			g_hash_table_remove(rf->activity, "main");
		}
	} else {
#if (EVOLUTION_VERSION >= 22900) //kb//
#if (EVOLUTION_VERSION >= 29102)
		e_activity_set_state (aid, E_ACTIVITY_COMPLETED);
		g_object_unref(aid);
#else
		e_activity_complete (aid);
#endif
#else
		e_activity_handler_operation_finished(activity_handler, aid);
#endif
		g_hash_table_remove(rf->activity, key);
	}
}

#if (EVOLUTION_VERSION >= 22900) //kb//
EActivity*
#else
guint
#endif
taskbar_op_message(gchar *msg, gchar *unikey)
{
		gchar *tmsg;
#if (EVOLUTION_VERSION >= 22900) //kb//
		EActivity *activity_id;
#else
#endif
		if (!msg) {
			tmsg = g_strdup_printf(
				_("Fetching Feeds (%d enabled)"),
				g_hash_table_size(rf->hrname));
			unikey = "main";
		} else
			tmsg = g_strdup(msg);

#if (EVOLUTION_VERSION >= 22900) //kb//
		if (!msg)
			activity_id =
				(EActivity *)taskbar_op_new(
					tmsg,
					unikey);
		else
			activity_id =
				(EActivity *)taskbar_op_new(tmsg, msg);
#else
		if (!msg)
			activity_id = taskbar_op_new(tmsg, unikey);
		else
			activity_id = taskbar_op_new(tmsg, msg);
#endif
		g_hash_table_insert(
			rf->activity,
			GUINT_TO_POINTER(unikey),
			GUINT_TO_POINTER(activity_id));
		g_free(tmsg);
		return activity_id;
}

