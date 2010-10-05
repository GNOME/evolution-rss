/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2010 Lucian Langa <cooly@gnome.eu.org>
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

int rss_verbose_debug = 0;

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#if (DATASERVER_VERSION >= 2031001)
#include <camel/camel.h>
#else
#include <camel/camel-mime-message.h>
#include <camel/camel-file-utils.h>
#include <camel/camel-folder.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-stream-buffer.h>
#include <camel/camel-text-index.h>
#include <camel/camel-medium.h>
#endif

#include <e-util/e-icon-factory.h>
#include <e-util/e-util.h>
#include <e-util/e-mktemp.h>


#ifdef EVOLUTION_2_12
#include <mail/em-event.h>
#endif

#include <mail/em-utils.h>
#include <mail/em-folder-tree.h>

#if EVOLUTION_VERSION < 22900 //kb//
#include <e-util/e-error.h>
#include <mail/em-popup.h>
#include <mail/em-folder-view.h>
#include <mail/em-format.h>
#include <mail/mail-component.h>
#include <misc/e-activity-handler.h>
#include <bonobo/bonobo-shlib-factory.h>
#else
#include <e-util/e-alert-dialog.h> //remove//
#include <glib/gi18n.h>
#include <mail/e-mail-local.h>
#include <mail/mail-session.h>
#include <shell/e-shell.h>
#include <shell/e-shell-view.h>
#include <misc/e-popup-menu.h>
#endif

#include <mail/mail-tools.h>
#include <mail/mail-ops.h>

#include <mail/em-format-html.h>
#include <mail/em-format-hook.h>

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <fcntl.h>
#include <stdlib.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#include <glib.h>
#include <gtk/gtk.h>

#include <shell/es-event.h>

#ifdef HAVE_GTKHTMLEDITOR
#include <editor/gtkhtml-editor.h>
#endif

#include <libxml/HTMLtree.h>

#ifdef HAVE_RENDERKIT
#ifdef HAVE_GECKO
#ifdef HAVE_GECKO_1_9
#include <gtkmozembed.h>
#else
#include <gtkembedmoz/gtkmozembed.h>
#endif
#include "gecko-utils.h"
#endif

#ifdef HAVE_OLD_WEBKIT
#include "webkitgtkglobal.h"
#include "webkitgtkpage.h"
#define webkit_web_view_stop_loading(obj) webkit_gtk_page_stop_loading(obj)
#define webkit_web_view_open(obj, data) webkit_gtk_page_open(obj, data)
#define webkit_web_view_new() webkit_gtk_page_new()
#else
	#ifdef HAVE_WEBKIT
	#ifdef WEBKIT_UNSTD
	#include <WebKit/webkitwebview.h>
	#else
	#include <webkit/webkitwebview.h>
	#endif
	#endif
#endif

#endif

#include <libsoup/soup.h>
#if LIBSOUP_VERSION < 2003000
#include <libsoup/soup-message-queue.h>
#endif

#include <sys/time.h>
#ifdef _WIN32
#include "strptime.c"
#endif

#include "rss.h"
#include "rss-config.h"
#include "rss-cache.h"
#include "rss-image.h"
#include "parser.h"
#include "network-soup.h"
#include "notification.h"
#include "file-gio.h"
#include "fetch.h"
#include "misc.h"
#if HAVE_DBUS
#include "dbus.h"
#endif
#include "rss-config-factory.h"
#include "rss-icon-factory.h"
#include "parser.h"



int pop = 0;
GtkWidget *flabel;
//#define RSS_DEBUG 1
guint nettime_id = 0;
guint force_update = 0;
GHashTable *custom_timeout;
GtkStatusIcon *status_icon = NULL;
GQueue *status_msg;
gchar *flat_status_msg;
GPtrArray *filter_uids;
gpointer current_pobject = NULL;
guint resize_pane_hsize = 0;
guint resize_pane_vsize = 0;
guint resize_browser_hsize = 0;
guint resize_browser_vsize = 0;
guint progress = 0;
gboolean feed_new = FALSE;	//make sense only for setting up a new feed
			//prevents jumping to old feeds when new articles found

extern guint net_queue_run_count;
extern guint net_qid;


static volatile int org_gnome_rss_controls_counter_id = 0;

struct _org_gnome_rss_controls_pobject {
	EMFormatHTMLPObject object;

	CamelMimePart *part;
	EMFormatHTML *format;
	GtkWidget *html;
	GtkWidget *container;
	GtkWidget *forwbut;		//browser forward button
	GtkWidget *backbut;		//browser back button
	GtkWidget *stopbut;		//browser stop button
	CamelStream *stream;
	gchar *website;
	guint is_html;
	gchar *mem;
	guint chandler;		//content handler_id
	guint sh_handler;		//size handler_id for horizontal
	guint counter;		//general counter for carring various numbers
};

GtkWidget *evo_window;
#if (EVOLUTION_VERSION < 22703)
static GdkPixbuf *folder_icon;
static gboolean initialised = FALSE;
#endif
GHashTable *icons = NULL;
#if (DATASERVER_VERSION >= 2023001)
extern EProxy *proxy;
#endif
SoupSession *webkit_session = NULL;
#if LIBSOUP_VERSION > 2024000
SoupCookieJar *rss_soup_jar;
#endif
extern guint rsserror;
gboolean single_pending = FALSE;
#if EVOLUTION_VERSION < 22900
extern CamelSession *session;
#endif

rssfeed *rf = NULL;
guint upgrade = 0;	// set to 2 when initailization successfull
guint count = 0;
gchar *buffer = NULL;
GConfClient *rss_gconf;

gboolean inhibit_read = FALSE;	//prevent mail selection when deleting folder
gboolean delete_op = FALSE;	//delete in progress
gchar *commstream = NULL;	//global comments stream
guint commcnt = 0;	//global number of comments
const gchar *commstatus = "";
GSList *comments_session = NULL;	//comments to be fetched queue
guint32 frame_colour;
guint32 content_colour;
guint32 text_colour;

gboolean gecko_ready = FALSE;
gboolean browser_fetching = 0;	//mycall event could be triggered
				//many times in first step (fetching)
gint browser_fill = 0;	//how much data currently written to browser

gchar *process_feed(RDF *r);
gboolean display_feed(RDF *r);
gchar *display_doc (RDF *r);
gchar *display_comments (RDF *r, EMFormatHTML *format);
void check_folders(void);
CamelMimePart *file_to_message(const char *name);
void check_feed_age(void);
void get_feed_age(RDF *r, gpointer name);
gboolean display_feed_async(gpointer key);
gboolean fetch_one_feed(gpointer key, gpointer value, gpointer user_data);
gboolean fetch_feed(gpointer key, gpointer value, gpointer user_data);
gboolean custom_fetch_feed(gpointer key, gpointer value, gpointer user_data);
void fetch_comments(gchar *url, EMFormatHTML *stream);

guint fallback_engine(void);

gchar *print_comments(gchar *url, gchar *stream, EMFormatHTML *format);
static void refresh_cb (GtkWidget *button, EMFormatHTMLPObject *pobject);

#ifdef HAVE_WEBKIT
void webkit_set_history(gchar *base);
#endif
gboolean show_webkit(GtkWidget *webkit);
void sync_folders(void);

GtkTreeStore *evolution_store = NULL;
#if EVOLUTION_VERSION >= 22900
EShellView *rss_shell_view = NULL;
#endif

/*======================================================================*/

gpointer
lookup_key(gpointer key)
{
	g_return_val_if_fail(key, NULL);

	return g_hash_table_lookup(rf->hrname, key);
}

void
compare_enabled(gpointer key, gpointer value, guint *data)
{
	if (GPOINTER_TO_INT(value) == 1)
		*data = *data+1;
}

guint
rss_find_enabled(void)
{
	guint enabled=0;
	g_hash_table_foreach (rf->hre, (GHFunc)compare_enabled, &enabled);
	return enabled;
}

/* hash table of ops->dialogue of active errors */
static GHashTable *active_errors = NULL;

void error_destroy(GtkObject *o, void *data)
{
	g_hash_table_remove(active_errors, data);
}

void error_response(GtkObject *o, int button, void *data)
{
	gtk_widget_destroy((GtkWidget *)o);
}


void
cancel_active_op(gpointer key)
{
	gpointer key_session = g_hash_table_lookup(rf->key_session, key);
	gpointer value = g_hash_table_lookup(rf->session, key_session);
	if (value)
		cancel_soup_sess(key_session, value, NULL);
}

static void
statuscb(NetStatusType status, gpointer statusdata, gpointer data)
{
//	rssfeed *rf = data;
	NetStatusProgress *progress;
	float fraction = 0;
	d("status:%d\n", status);

	switch (status) {
		case NET_STATUS_BEGIN:
			g_print("NET_STATUS_BEGIN\n");
		break;
		case NET_STATUS_PROGRESS:
			progress = (NetStatusProgress*)statusdata;
			if (progress->current > 0 && progress->total > 0) {
				fraction = (float)progress->current / progress->total;
				while (gtk_events_pending ())
				gtk_main_iteration ();
			if (rf->cancel_all) break;
#ifndef EVOLUTION_2_12
		if (rf->progress_dialog  && 0 <= fraction && 1 >= fraction) {
			gtk_progress_bar_set_fraction(
				(GtkProgressBar *)rf->progress_bar,
				fraction);
			gchar *what = g_strdup_printf(
					_("%2.0f%% done"),
					fraction*100);
			gtk_label_set_text(GTK_LABEL(rf->label), data);
			gtk_progress_bar_set_text(
				(GtkProgressBar *)rf->progress_bar,
				what);
			g_free(what);
		}
#else
		if (rf->progress_bar && 0 <= fraction && 1 >= fraction) {
			gtk_progress_bar_set_fraction(
				(GtkProgressBar *)rf->progress_bar,
				fraction);
		}
		if (rf->sr_feed) {
			gchar *furl = g_markup_printf_escaped(
					"<b>%s</b>: %s",
					_("Feed"),
					(char *)data);
			gtk_label_set_markup (
				GTK_LABEL (rf->sr_feed),
				furl);
			g_free(furl);
		}
#endif
			}
	//update individual progress if previous percetage has not changed
			if (rf->progress_bar && rf->feed_queue) {
				gtk_progress_bar_set_fraction(
					(GtkProgressBar *)rf->progress_bar,
					(double)(100-rf->feed_queue*100/rss_find_enabled())/100);
			}
		break;
		case NET_STATUS_DONE:
			//progress_window_set_cancel_cb(pw, NULL, NULL);
			//progress_window_set_progress(pw, -1);
			g_print("NET_STATUS_DONE\n");
		break;
		default:
			g_warning("unhandled network status %d\n", status);
	}
}

void
update_progress_text(gchar *title)
{
	GtkWidget *label;

	if (!rf->progress_bar)
		return;

	label = g_object_get_data((GObject *)rf->progress_bar, "label");
	if (label) {
		gtk_label_set_text(
			GTK_LABEL(label), title);
		gtk_label_set_ellipsize (
			GTK_LABEL (label),
			PANGO_ELLIPSIZE_START);
		gtk_label_set_justify(
			GTK_LABEL(label),
			GTK_JUSTIFY_CENTER);
	}
}

void
update_progress_bar(guint current);

void
update_progress_bar(guint current)
{
	gdouble fr;
	gchar *what;
	guint total;

	g_return_if_fail(rf->progress_bar != NULL);

	total = GPOINTER_TO_INT(g_object_get_data(
				(GObject *)rf->progress_bar,
				"total"));
	if (total) {
	fr = ((progress*100)/total);
	if (fr < 100)
		gtk_progress_bar_set_fraction(
			(GtkProgressBar *)rf->progress_bar, fr/100);
	what = g_strdup_printf(_("%2.0f%% done"), fr);
	gtk_progress_bar_set_text(
		(GtkProgressBar *)rf->progress_bar, what);
	g_free(what);
	}
}


void
browser_write(gchar *string, gint length, gchar *base)
{
	gchar *str = string;
	guint engine = fallback_engine();
	switch (engine) {
	case 2:
#ifdef HAVE_GECKO
	gtk_moz_embed_open_stream((GtkMozEmbed *)rf->mozembed,
			base, "text/html");
	while (length > 0) {
		if (length > 4096) {
			gtk_moz_embed_append_data(
				(GtkMozEmbed *)rf->mozembed,
				str, 4096);
			str+=4096;
		} else
			gtk_moz_embed_append_data(
				(GtkMozEmbed *)rf->mozembed,
				str, length);
	length-=4096;
	}
	gtk_moz_embed_close_stream((GtkMozEmbed *)rf->mozembed);
#endif
	break;
	case 1:
#ifdef HAVE_WEBKIT
#if (DATASERVER_VERSION >= 2023001)
		proxify_webkit_session(proxy, base);
#endif

		webkit_web_view_load_string(
			WEBKIT_WEB_VIEW(rf->mozembed),
			str,
			"text/html",
			NULL,
			base);
		if (strncmp(base, "file:///fake", 12))
			webkit_set_history(base);
#endif
		break;
	}
}

void
browser_stream_write(CamelStream *stream, gchar *base);

void
browser_stream_write(CamelStream *stream, gchar *base)
{
	GString *str = g_string_new(NULL);
	gchar *line;
	CamelStream *in = camel_stream_buffer_new(stream, CAMEL_STREAM_BUFFER_READ);
	while ((line = camel_stream_buffer_read_line((CamelStreamBuffer *)in))) {
		gchar *tmp = line;
		g_string_append(str, line);
		g_free(tmp);
		line = NULL;
	}
#ifdef HAVE_WEBKIT
#if (WEBKIT_VERSION >= 1001001)
	webkit_web_view_load_string(
		WEBKIT_WEB_VIEW(rf->mozembed),
		str->str,
		"text/html",
		NULL,
		base);
#else
	webkit_web_view_load_html_string(
		WEBKIT_WEB_VIEW(rf->mozembed),
		str->str,
		base);
#endif
#endif
	g_string_free(str, 1);
#if (DATASERVER_VERSION >= 2031001)
	g_object_unref(in);
#else
	camel_object_unref(in);
#endif
#ifdef HAVE_WEBKIT
	webkit_set_history(base);
#endif
}

static void
browsercb(NetStatusType status, gpointer statusdata, gpointer data)
{
	//NetStatusProgress *progress = (NetStatusProgress*)statusdata;
	switch (status) {
	case NET_STATUS_PROGRESS:
#if 0
//		g_print("chunk:%s\n", progress->chunk);
		g_print("total:%d\n", progress->total);
		g_print("curent:%d\n", progress->current);
		g_print("-------------- chunk: %d =============\n", GPOINTER_TO_INT(progress->chunksize));
		//browser_write(progress->chunk, progress->chunksize, data);
//		browser_fill+=progress->chunksize;
#endif
	break;
	default:
		g_warning("unhandled network status %d\n", status);
	}
}

void
textcb(NetStatusType status, gpointer statusdata, gpointer data)
{
	NetStatusProgress *progress;
	float fraction = 0;
	switch (status) {
	case NET_STATUS_PROGRESS:
		progress = (NetStatusProgress*)statusdata;
		if (progress->current > 0 && progress->total > 0) {
			fraction = (float)progress->current / progress->total;
			g_print("%.2f%% ", fraction);
		}
		while (gtk_events_pending())
			gtk_main_iteration ();
		break;
	default:
		g_warning("unhandled network status %d\n", status);
	}
}

void
download_chunk(
	NetStatusType status,
	gpointer statusdata,
	gpointer data)
{
	NetStatusProgress *progress;
	//float fraction = 0;
	switch (status) {
	case NET_STATUS_PROGRESS:
		progress = (NetStatusProgress*)statusdata;
		if (progress->current > 0 && progress->total > 0) {
			//fraction = (float)progress->current / progress->total;
			fwrite(progress->chunk, 1, progress->chunksize, (FILE *)data);
		}
		break;
	default:
		g_warning("unhandled network status %d\n", status);
	}
}

void
user_pass_cb(RSS_AUTH *auth_info, gint response, GtkDialog *dialog)
{
	switch (response) {
	case GTK_RESPONSE_OK:
		if (auth_info->user)
			g_hash_table_remove(rf->hruser, auth_info->url);

		g_hash_table_insert(
			rf->hruser, g_strdup(auth_info->url),
			g_strdup(gtk_entry_get_text (GTK_ENTRY (auth_info->username))));

		if (auth_info->pass)
			g_hash_table_remove(rf->hrpass, auth_info->url);

		g_hash_table_insert(rf->hrpass, g_strdup(auth_info->url),
			g_strdup(gtk_entry_get_text (GTK_ENTRY (auth_info->password))));

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (auth_info->rememberpass)))
			save_up(auth_info->url);
		else
			del_up(auth_info->url);

		rf->soup_auth_retry = FALSE;
		auth_info->user = g_hash_table_lookup(
					rf->hruser, auth_info->url);
		auth_info->pass = g_hash_table_lookup(
					rf->hrpass, auth_info->url);
		if (!auth_info->retrying)
			soup_auth_authenticate (auth_info->soup_auth,
					auth_info->user,
					auth_info->pass);
		break;
	default:
		rf->soup_auth_retry = TRUE;
			soup_session_abort(auth_info->session);
		goto out;
	}
	if (G_OBJECT_TYPE(auth_info->session) == SOUP_TYPE_SESSION_ASYNC) {
		soup_session_unpause_message(
			auth_info->session, auth_info->message);
	}
out:	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_free(auth_info);

}

GtkDialog *
create_user_pass_dialog(RSS_AUTH *auth)
{
	GtkWidget *username;
	GtkWidget *password;
	GtkWidget *checkbutton1;
	GtkWidget *container, *container2;
	GtkWidget *widget, *action_area;
	GtkWidget *content_area, *password_dialog;
	AtkObject *a11y;
	gchar *markup;

	widget = gtk_dialog_new_with_buttons (
		_("Enter User/Pass for feed"), NULL, 0,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);
#if GTK_MINOR_VERSION < 22
	gtk_dialog_set_has_separator (GTK_DIALOG (widget), FALSE);
#endif
	gtk_dialog_set_default_response (
		GTK_DIALOG (widget), GTK_RESPONSE_OK);
	gtk_window_set_resizable (GTK_WINDOW (widget), FALSE);
//        gtk_window_set_transient_for (GTK_WINDOW (widget), widget->parent);
	gtk_window_set_position (
		GTK_WINDOW (widget),
		GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
	password_dialog = GTK_WIDGET (widget);

#if GTK_VERSION >= 2014000
	action_area = gtk_dialog_get_action_area (GTK_DIALOG(password_dialog));
	content_area = gtk_dialog_get_content_area (GTK_DIALOG(password_dialog));
#else
	action_area = GTK_DIALOG (password_dialog)->action_area;
	content_area = NULL;
#endif

	/* Override GtkDialog defaults */
	gtk_box_set_spacing (GTK_BOX (action_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 0);
	gtk_box_set_spacing (GTK_BOX (content_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);

	/* Table */
	container = gtk_table_new (2, 3, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (container), 12);
	gtk_table_set_row_spacings (GTK_TABLE (container), 6);
	gtk_table_set_row_spacing (GTK_TABLE (container), 0, 12);
	gtk_table_set_row_spacing (GTK_TABLE (container), 1, 0);
	gtk_widget_show (container);

	gtk_box_pack_start (
		GTK_BOX (content_area), container, FALSE, TRUE, 0);

	/* Password Image */
	widget = gtk_image_new_from_icon_name (
		"dialog-password", GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.0);
	gtk_widget_show (widget);

	gtk_table_attach (
		GTK_TABLE (container), widget,
		0, 1, 0, 3, GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	widget = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);

	markup = g_markup_printf_escaped (
			_("Enter your username and password for:\n '%s'"),
			auth->url);
	gtk_label_set_markup (GTK_LABEL (widget), markup);
	g_free (markup);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_widget_show (widget);

	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	container2 = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (container2);
	gtk_table_attach (
		GTK_TABLE (container), container2,
		1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	widget = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (widget), _("Username: "));
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_widget_show (widget);
	gtk_table_attach (
		GTK_TABLE (container2), widget,
		0, 1, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	username = gtk_entry_new ();
	a11y = gtk_widget_get_accessible (username);
	gtk_entry_set_visibility (GTK_ENTRY (username), TRUE);
	gtk_entry_set_activates_default (GTK_ENTRY (username), TRUE);
	gtk_widget_grab_focus (username);
	gtk_widget_show (username);
	gtk_table_attach (
		GTK_TABLE (container2), username,
		1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	if (auth->user)
		gtk_entry_set_text (GTK_ENTRY (username), auth->user);
	auth->username = username;


	widget = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (widget), _("Password: "));
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_widget_show (widget);
	gtk_table_attach (
		GTK_TABLE (container2), widget,
		0, 1, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	password = gtk_entry_new ();
	a11y = gtk_widget_get_accessible (password);
	gtk_entry_set_visibility (GTK_ENTRY (password), FALSE);
	gtk_entry_set_activates_default (GTK_ENTRY (password), TRUE);
	gtk_widget_grab_focus (password);
	gtk_widget_show (password);
	gtk_table_attach (
		GTK_TABLE (container2), password,
		1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	if (auth->pass)
		gtk_entry_set_text (GTK_ENTRY (password), auth->pass);
	auth->password = password;

	/* Caps Lock Label */
	widget = gtk_label_new (NULL);
//        update_capslock_state (NULL, NULL, widget);
	gtk_widget_show (widget);

	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);

//        g_signal_connect (
//              password_dialog, "key-release-event",
//            G_CALLBACK (update_capslock_state), widget);
//  g_signal_connect (
//        password_dialog, "focus-in-event",
//      G_CALLBACK (update_capslock_state), widget);


	checkbutton1  = gtk_check_button_new_with_mnemonic (
				_("_Remember this password"));

//                gtk_toggle_button_set_active (
//                      GTK_TOGGLE_BUTTON (widget), *msg->remember);
//            if (msg->flags & E_PASSWORDS_DISABLE_REMEMBER)
//                 gtk_widget_set_sensitive (widget, FALSE);
	gtk_widget_show (checkbutton1);
	auth->rememberpass = checkbutton1;

	gtk_table_attach (
		GTK_TABLE (container), checkbutton1,
		1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	gtk_widget_show_all(password_dialog);
	return GTK_DIALOG(password_dialog);
}

void
web_auth_dialog(RSS_AUTH *auth_info)
{
	GtkDialog *dialog;
	gint response;

	if (!rf->hruser)
		rf->hruser = g_hash_table_new_full(
				g_str_hash, g_str_equal, g_free, g_free);
	if (!rf->hrpass)
		rf->hrpass = g_hash_table_new_full(
				g_str_hash, g_str_equal, g_free, g_free);

	d("auth url:%s\n", auth_info->url);
	auth_info->user = g_hash_table_lookup(rf->hruser, auth_info->url);
	auth_info->pass = g_hash_table_lookup(rf->hrpass, auth_info->url);
	d("auth user:%s\n", auth_info->user);
	d("auth pass:%s\n", auth_info->pass);
	dialog = create_user_pass_dialog(auth_info);
	//Bug 522147 – need to be able to pause synchronous I/O
	if (G_OBJECT_TYPE(auth_info->session) != SOUP_TYPE_SESSION_ASYNC) {
		response = gtk_dialog_run(dialog);
		user_pass_cb(auth_info, response, dialog);
	} else
		g_signal_connect_swapped (dialog,
			"response",
			G_CALLBACK (user_pass_cb),
			auth_info);
}

gboolean
proxy_auth_dialog(gchar *title, gchar *user, gchar *pass)
{
	GtkDialog *dialog;
	gint result;

	RSS_AUTH *auth_info = g_new0(RSS_AUTH, 1);
	auth_info->user = user;
	auth_info->pass = pass;
	dialog = create_user_pass_dialog(auth_info);
	result = gtk_dialog_run(GTK_DIALOG(dialog));
	/*LEAK g_free(auth_info);*/
	return TRUE;
}

gboolean
timeout_soup(void)
{
	d("Network timeout occured. Cancel active operations.\n");
	abort_all_soup();
	return FALSE;
}

void
network_timeout(void)
{
	float timeout;

	if (nettime_id)
		g_source_remove(nettime_id);

	timeout = gconf_client_get_float(
			rss_gconf, GCONF_KEY_NETWORK_TIMEOUT, NULL);

	if (!timeout)
		timeout = NETWORK_MIN_TIMEOUT;

	nettime_id = g_timeout_add (
				(guint)(timeout)*1000,
				(GtkFunction) timeout_soup,
				0);
}

static void
readrss_dialog_cb (GtkWidget *widget, gpointer data)
{
	d("\nCancel reading feeds\n");
	abort_all_soup();
#ifndef EVOLUTION_2_12
	gtk_widget_destroy(widget);
	rf->progress_dialog = NULL;
#endif
	rf->cancel = 1;
}

static void
receive_cancel(GtkButton *button, struct _send_info *info)
{
	if (info->state == SEND_ACTIVE) {
		if (info->status_label)
			gtk_label_set_markup (GTK_LABEL (info->status_label),
//                        e_clipped_label_set_text (
//                              E_CLIPPED_LABEL (info->status_label),
				_("Canceling..."));
		info->state = SEND_CANCELLED;
		readrss_dialog_cb(NULL, NULL);
	}
	if (info->cancel_button)
		gtk_widget_set_sensitive(info->cancel_button, FALSE);

//	abort_all_soup();
}

void
rss_select_folder(gchar *folder_name)
{
#if EVOLUTION_VERSION >= 22900
	EMFolderTree *folder_tree = NULL;
	gchar *uri;
	CamelStore *store;
	CamelFolder *fold = NULL;
	EShellSidebar *shell_sidebar;

	d("rss_select_folder() %s:%d\n", __FILE__, __LINE__);

	g_return_if_fail(folder_name != NULL);

	shell_sidebar  = e_shell_view_get_shell_sidebar(rss_shell_view);
	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);
	store = rss_component_peek_local_store();
	fold = camel_store_get_folder (store, folder_name, 0, NULL);
	if (!fold) return;
	uri = mail_tools_folder_to_url (fold);
	em_folder_tree_set_selected(folder_tree, uri, 0);
#endif
#if 0 //kb//
	CamelStore *store = rss_component_peek_local_store();
	EMFolderTreeModel *model = mail_component_peek_tree_model(mail_component_peek());
	gchar *real_name = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", lookup_main_folder(), folder_name);
	CamelFolder *folder = camel_store_get_folder (store, real_name, 0, NULL);

	g_print("real_name:%s\n", real_name);
	char *uri = mail_tools_folder_to_url (folder);
	g_print("uri:%s\n", uri);
	g_print("selected:%s\n", em_folder_tree_model_get_selected (model));
	em_folder_tree_model_set_selected (model, uri);
	g_print("selected:%s\n", em_folder_tree_model_get_selected (model));
//	 refresh_folder_tree (model, store);

/*	MailComponent *mail_component = mail_component_peek();
	MailComponentPrivate *priv = mail_component->priv;
	EComponentView *cv = priv->component_view;
	g_print("priv:%p", priv);
	g_print("cv:%p", cv);*/
//	void *el = g_object_get_data((GObject *)cv, "info-label");
//      EMFolderView *emfv = g_object_get_data((GObject *)el, "folderview");
//	EMFolderView *emfv = g_object_new(em_folder_view_get_type(), NULL);
//	GtkWidget *po = (GtkWidget *)model.parent_object;
//      em_folder_tree_set_selected ((EMFolderView *)po), uri, FALSE);
//	camel_operation_end(NULL);
	g_free(uri);
	camel_object_unref (folder);
	g_free(real_name);
#endif
}

static void
summary_cb (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	rf->cur_format = rf->cur_format^1;
	rf->chg_format = 1;
	em_format_redraw((EMFormat *)pobject);
//	while (gtk_events_pending ())
//           gtk_main_iteration ();
}


static void
back_cb (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	guint engine;
	engine = fallback_engine();
#ifdef	HAVE_GECKO
	if (engine == 2)
		gtk_moz_embed_go_back((GtkMozEmbed *)rf->mozembed);
#endif
#ifdef HAVE_WEBKIT
	if (engine == 1) {
		webkit_web_view_go_back (WEBKIT_WEB_VIEW(rf->mozembed));
	}
#endif
}

static void
forward_cb (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	guint engine;
	engine = fallback_engine();
#ifdef	HAVE_GECKO
	if (engine == 2)
		gtk_moz_embed_go_forward((GtkMozEmbed *)rf->mozembed);
#endif
#ifdef HAVE_WEBKIT
	if (engine == 1)
		webkit_web_view_go_forward(WEBKIT_WEB_VIEW(rf->mozembed));
#endif
}

void stop_cb (GtkWidget *button, EMFormatHTMLPObject *pobject);

void
stop_cb (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	guint engine;
	engine = fallback_engine();
#ifdef	HAVE_GECKO
	if (engine == 2)
		gtk_moz_embed_stop_load((GtkMozEmbed *)rf->mozembed);
#endif
#ifdef HAVE_WEBKIT
	if (engine == 1)
		webkit_web_view_stop_loading(WEBKIT_WEB_VIEW(rf->mozembed));
#endif
}

void
reload_cb (GtkWidget *button, gpointer data)
{
	guint engine = fallback_engine();
	switch (engine) {
		case 2:
#ifdef	HAVE_GECKO
	gtk_moz_embed_stop_load((GtkMozEmbed *)rf->mozembed);
	gtk_moz_embed_load_url ((GtkMozEmbed *)rf->mozembed, data);
#endif
		break;
		case 1:
#ifdef	HAVE_WEBKIT
#if (WEBKIT_VERSION >= 1000003)
	webkit_web_view_reload_bypass_cache(WEBKIT_WEB_VIEW(rf->mozembed));
#else
	webkit_web_view_reload(WEBKIT_WEB_VIEW(rf->mozembed));
#endif
#endif
		break;
	}
}

typedef struct _UB {
	CamelStream *stream;
	gchar *url;
	gboolean create;
} UB;

void
rss_browser_set_hsize (GtkAdjustment *adj, gpointer data);

void
rss_browser_set_hsize (GtkAdjustment *adj, gpointer data)
{
#if GTK_VERSION >= 2014000
	resize_pane_hsize = gtk_adjustment_get_page_size(adj);
#else
	resize_pane_hsize = (adj->page_size);
#endif
}

void rss_browser_update_content (
	GtkWidget *widget, GtkAllocation *event, gpointer data);

void
rss_browser_update_content (
	GtkWidget *widget, GtkAllocation *event, gpointer data)
{
	struct _org_gnome_rss_controls_pobject *po = data;
	CamelStream *stream = NULL;
	UB *fi;

	if (GTK_IS_WIDGET(widget)) {
		if (rf->mozembed)
			if (
#if GTK_VERSION >= 2019007
			gtk_widget_get_realized(rf->mozembed)
#else
			GTK_WIDGET_REALIZED(rf->mozembed)
#endif
			) {
				if (!browser_fetching) {
					gchar *msg = g_strdup_printf(
							"<h5>%s</h5>",
							_("Formatting Message..."));
//					browser_write(
//						msg, strlen(msg),
//						(gchar *)"file:///fakefile#index");
					g_free(msg);
					browser_fetching=1;
					fi = g_new0(UB, 1);
					stream = rss_cache_get(po->website);
					if (!stream) {
						d("HTTP cache miss\n");
						stream = rss_cache_add(po->website);
						fi->create = 1;
					fi->stream = stream;
					fi->url = g_strdup(po->website);
					fetch_unblocking(
						po->website,
						browsercb,
						po->website,
						(gpointer)finish_website,
						fi,
						1,
						NULL);
					/*FIXME free fi*/
					} else {
						d("cache read\n");
						fi->create = 0;
						browser_stream_write(stream, po->website);
						camel_stream_close(stream);
#if (DATASERVER_VERSION >= 2031001)
						g_object_unref(stream);
#else
						camel_object_unref(stream);
#endif
					}
				}
			}
	}
}

#ifdef HAVE_GECKO
void
rss_mozilla_init(void)
{
	d("rss_mozilla_init() called in %s:%d\n", __FILE__, __LINE__);
	if (!gecko_ready) {
		gecko_init();
		gecko_ready = 1;
	}
}
#endif

void
webkit_set_preferences(void)
{
#ifdef HAVE_WEBKIT
	gchar *agstr;
	WebKitWebSettings *settings;
#if (WEBKIT_VERSION >= 1001011)
#endif
#if (WEBKIT_VERSION >= 1001001)
	webkit_session = webkit_get_default_session();
	if (rss_soup_jar)
		soup_session_add_feature(
			webkit_session, SOUP_SESSION_FEATURE(rss_soup_jar));
#endif
#if (WEBKIT_VERSION >= 1001011)
	settings = webkit_web_view_get_settings(
			WEBKIT_WEB_VIEW(rf->mozembed));
	agstr = g_strdup_printf("Evolution/%s; Evolution-RSS/%s",
			EVOLUTION_VERSION_STRING, VERSION);
	g_object_set (settings, "user-agent", agstr,  NULL);
#if (WEBKIT_VERSION >= 1001022)
	g_object_set (settings, "enable-page-cache", TRUE, NULL);
	//g_object_set (settings, "auto-resize-window", TRUE, NULL);
	g_object_set (settings, "enable-plugins",
		gconf_client_get_bool(rss_gconf, GCONF_KEY_EMBED_PLUGIN, NULL),
		NULL);
	g_object_set (settings, "enable-java-applet",
		gconf_client_get_bool(rss_gconf, GCONF_KEY_HTML_JAVA, NULL),
		NULL);
	g_object_set (settings, "enable-scripts",
		gconf_client_get_bool(rss_gconf, GCONF_KEY_HTML_JS, NULL),
		NULL);
#endif
	webkit_web_view_set_full_content_zoom(
		(WebKitWebView *)rf->mozembed, TRUE);
	g_free(agstr);
#endif
#endif
}

void
gecko_set_preferences(void)
{
#ifdef HAVE_GECKO
	SoupURI *uri;
	gchar *agstr;

	gecko_prefs_set_bool("javascript.enabled",
		gconf_client_get_bool(rss_gconf, GCONF_KEY_HTML_JS, NULL));
	gecko_prefs_set_bool("security.enable_java",
		gconf_client_get_bool(rss_gconf, GCONF_KEY_HTML_JAVA, NULL));
	gecko_prefs_set_bool("plugin.scan.plid.all", FALSE);
	gecko_prefs_set_bool("plugin.default_plugin_disabled", TRUE);
	agstr = g_strdup_printf("Evolution/%s; Evolution-RSS/%s",
			EVOLUTION_VERSION_STRING, VERSION);
	gecko_prefs_set_string("general.useragent.extra.firefox", agstr);
	gecko_prefs_set_int("browser.ssl_override_behaviour", 2);
	gecko_prefs_set_bool("browser.xul.error_pages.enabled", FALSE);
	gecko_prefs_set_bool("browser.xul.error_pages.expert_bad_cert", FALSE);
	g_free(agstr);
#if (DATASERVER_VERSION >= 2026000)
	//I'm only forcing scheme here
	uri = e_proxy_peek_uri_for(proxy, "http:///");

	if (uri) {
		gecko_prefs_set_string("network.proxy.http", uri->host);
		gecko_prefs_set_int("network.proxy.http_port", uri->port);
		gecko_prefs_set_int("network.proxy.type", 1);
	}
#else
	g_print("WARN e_proxy_peek_uri_for() requires evolution-data-server 2.26\n");
	return;
#endif
//	soup_uri_free(uri);
//	uri = e_proxy_peek_uri_for(proxy, "https:///");
//	gecko_prefs_set_string("network.proxy.ssl", uri->host);
//	gecko_prefs_set_int("network.proxy.ssl_port", uri->port);
//	soup_uri_free(uri);
#endif
}

#ifdef HAVE_GECKO
static void
#if EVOLUTION_VERSION < 22900
rss_popup_zoom_in(EPopup *ep, EPopupItem *pitem, void *data)
#else
rss_popup_zoom_in(GtkWidget *widget, gpointer data)
#endif
{
	gfloat zoom = gecko_get_zoom(rf->mozembed);
	zoom*=1.2;
	gecko_set_zoom(rf->mozembed, zoom);
}

static void
#if EVOLUTION_VERSION < 22900
rss_popup_zoom_out(EPopup *ep, EPopupItem *pitem, void *data)
#else
rss_popup_zoom_out(GtkWidget *widget, gpointer data)
#endif
{
	gfloat zoom = gecko_get_zoom(rf->mozembed);
	zoom/=1.2;
	gecko_set_zoom(rf->mozembed, zoom);
}

static void
#if EVOLUTION_VERSION < 22900
rss_popup_zoom_orig(EPopup *ep, EPopupItem *pitem, void *data)
#else
rss_popup_zoom_orig(GtkWidget *widget, gpointer data)
#endif
{
	gecko_set_zoom(rf->mozembed, 1);
}

static void
#if EVOLUTION_VERSION < 22900
rss_popup_link_copy(EPopup *ep, EPopupItem *pitem, void *data)
#else
rss_popup_link_copy(GtkWidget *widget, gpointer data)
#endif
{
	gtk_clipboard_set_text (
		gtk_clipboard_get (GDK_SELECTION_PRIMARY),
		data, -1);
	gtk_clipboard_set_text (
		gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
		data, -1);
}

static void
#if EVOLUTION_VERSION < 22900
rss_popup_link_open(EPopup *ep, EPopupItem *pitem, void *data)
#else
rss_popup_link_open(GtkWidget *widget, gpointer data)
#endif
{
#if (EVOLUTION_VERSION >= 22505)
	e_show_uri (NULL, data);
#endif
}

void
browser_copy_selection(GtkWidget *widget, gpointer data)
{
	gecko_copy_selection((GtkMozEmbed *)rf->mozembed);
}

void
browser_select_all(GtkWidget *widget, gpointer data)
{
	gecko_select_all((GtkMozEmbed *)rf->mozembed);
}

#if EVOLUTION_VERSION >= 22900
EPopupMenu rss_menu_items[] = {
	E_POPUP_ITEM (N_("_Copy"),		G_CALLBACK(browser_copy_selection), 1),
	E_POPUP_ITEM (N_("Select _All"),	G_CALLBACK(browser_select_all), 1),
	E_POPUP_SEPARATOR,
	E_POPUP_ITEM (N_("Zoom _In"),		G_CALLBACK(rss_popup_zoom_in), 2),
	E_POPUP_ITEM (N_("Zoom _Out"),		G_CALLBACK(rss_popup_zoom_out), 2),
	E_POPUP_ITEM (N_("_Normal Size"),	G_CALLBACK(rss_popup_zoom_orig), 2),
	E_POPUP_SEPARATOR,
	E_POPUP_ITEM (N_("_Open Link"),		G_CALLBACK(rss_popup_link_open), 4),
	E_POPUP_ITEM (N_("_Copy Link Location"),G_CALLBACK(rss_popup_link_copy), 4),
	E_POPUP_TERMINATOR
};
#else
EPopupItem rss_menu_items[] = {
	{ E_POPUP_BAR, "05.rss-browser.01", NULL, NULL, NULL, NULL },
	{ E_POPUP_ITEM, "05.rss-browser.02", N_("Zoom _In"), rss_popup_zoom_in, NULL, "zoom-in", EM_POPUP_URI_HTTP },
	{ E_POPUP_ITEM, "05.rss-browser.03", N_("Zoom _Out"), rss_popup_zoom_out, NULL, "zoom-out", EM_POPUP_URI_HTTP },
	{ E_POPUP_ITEM, "05.rss-browser.04", N_("_Normal Size"), rss_popup_zoom_orig, NULL, "zoom-original", EM_POPUP_URI_HTTP },
	{ E_POPUP_BAR, "05.rss-browser.05", NULL, NULL, NULL, NULL },
	{ E_POPUP_ITEM, "05.rss-browser.06", N_("_Print..."), NULL, NULL, "document-print", EM_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "05.rss-browser.07", N_("Save _As"), NULL, NULL, "document-save-as", 0},
	{ E_POPUP_BAR, "05.rss-browser.08", NULL, NULL, NULL, NULL },
	{ E_POPUP_ITEM, "05.rss-browser.09", N_("_Open Link in Browser"), rss_popup_link_open, NULL, NULL, EM_POPUP_URI_HTTP },
	{ E_POPUP_ITEM, "05.rss-browser.10", N_("_Copy Link Location"), rss_popup_link_copy, NULL, "edit-copy" },
};

void
rss_menu_items_free(EPopup *ep, GSList *items, void *data);

void
rss_menu_items_free(EPopup *ep, GSList *items, void *data)
{
	g_slist_free(items);
}
#endif
#endif

#ifdef HAVE_WEBKIT
#if (WEBKIT_VERSION >= 1001007)
static void
webkit_net_status (WebKitWebView *view,
		GParamSpec *spec,
		GtkWidget *data)
{
	GtkAllocation alloc;
	gint width;
	WebKitLoadStatus status = webkit_web_view_get_load_status (view);
	switch (status) {
		case WEBKIT_LOAD_FINISHED:
			gtk_widget_set_sensitive(data, FALSE);
			if (rf->mozembed) {
				gtk_widget_get_allocation(rf->mozembed, &alloc);
				width = alloc.width;
				if (resize_pane_hsize > width && width != 1)
					gtk_widget_set_size_request(rf->mozembed,
						(int)resize_pane_hsize-14, -1);
			}
		break;
		default:
			gtk_widget_set_sensitive(data, TRUE);
		break;
	}
}

#endif


void
webkit_set_history(gchar *base)
{
	WebKitWebBackForwardList *back_forward_list =
		webkit_web_view_get_back_forward_list (WEBKIT_WEB_VIEW(rf->mozembed));
	WebKitWebHistoryItem *item =
		webkit_web_history_item_new_with_data(base, "Untitled");
#if (WEBKIT_VERSION >= 1001001)
	webkit_web_back_forward_list_add_item(back_forward_list, item);
#endif
}

#if (WEBKIT_VERSION >= 1001007)
static void
webkit_history_status (WebKitWebView *view,
		GParamSpec *spec,
		GtkWidget *data)
{
	struct _org_gnome_rss_controls_pobject *po =
			(struct _org_gnome_rss_controls_pobject *) data;
	WebKitLoadStatus status = webkit_web_view_get_load_status (view);
	switch (status) {
		case WEBKIT_LOAD_COMMITTED:
		default:
			if (!webkit_web_view_can_go_forward(view))
				gtk_widget_set_sensitive(po->forwbut, FALSE);
			else
				gtk_widget_set_sensitive(po->forwbut, TRUE);
			if (!webkit_web_view_can_go_back(view))
				gtk_widget_set_sensitive(po->backbut, FALSE);
			else
				gtk_widget_set_sensitive(po->backbut, TRUE);
		break;
	}
}
#endif

gboolean
webkit_over_link(WebKitWebView *web_view,
		gchar         *title,
		gchar         *uri,
		gpointer       user_data)
{
	if (uri) {
		gchar *msg = g_strdup_printf("%s %s", _("Click to open"), uri);
		taskbar_push_message(msg);
		g_free(msg);
	} else
		taskbar_pop_message();
	return TRUE;
}

static void
embed_zoom_in_cb (GtkWidget *w,
			gpointer *data)
{
	webkit_web_view_zoom_in((WebKitWebView*)rf->mozembed);
}

static void
embed_zoom_out_cb (GtkWidget *w,
			gpointer *data)
{
	webkit_web_view_zoom_out((WebKitWebView *)rf->mozembed);
}

static void
embed_zoom_100_cb (GtkWidget *w,
			gpointer *data)
{
	webkit_web_view_set_zoom_level((WebKitWebView *)rf->mozembed, 1);
}

gboolean
webkit_click (GtkEntry *entry,
		GtkMenu *menu,
		gpointer user_data)
{
	GtkWidget *separator, *menuitem;
	separator = gtk_separator_menu_item_new ();
	gtk_widget_show (separator);
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), separator, 2);
	menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_ZOOM_IN, NULL);
	g_signal_connect(menuitem, "activate",
		(GCallback)embed_zoom_in_cb, NULL);
	gtk_widget_set_sensitive (menuitem, TRUE);
	gtk_widget_show (menuitem);
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 3);
	menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_ZOOM_OUT, NULL);
	g_signal_connect(menuitem, "activate",
		(GCallback)embed_zoom_out_cb, NULL);
	gtk_widget_set_sensitive (menuitem, TRUE);
	gtk_widget_show (menuitem);
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 4);
	menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_ZOOM_100, NULL);
	g_signal_connect(menuitem, "activate",
		(GCallback)embed_zoom_100_cb, NULL);
	gtk_widget_set_sensitive (menuitem, TRUE);
	gtk_widget_show (menuitem);
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 5);
	separator = gtk_separator_menu_item_new ();
	gtk_widget_show (separator);
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), separator, 6);
	return TRUE;
}

void webkit_hook_actions(void);

void
webkit_hook_actions(void)
{
#if EVOLUTION_VERSION >= 22900
	EShellWindow *shell_window;
	GtkAction *action;
	const char *action_name;

	shell_window = e_shell_view_get_shell_window (rss_shell_view);

	action_name = "mail-zoom-in";
	action = e_shell_window_get_action (shell_window, action_name);
	g_signal_connect (
		action, "activate",
		G_CALLBACK (embed_zoom_in_cb), NULL);
	action_name = "mail-zoom-out";
	action = e_shell_window_get_action (shell_window, action_name);
	g_signal_connect (
		action, "activate",
		G_CALLBACK (embed_zoom_out_cb), NULL);
	action_name = "mail-zoom-100";
	action = e_shell_window_get_action (shell_window, action_name);
	g_signal_connect (
		action, "activate",
		G_CALLBACK (embed_zoom_100_cb), NULL);
#endif
}

#if 0
#if EVOLUTION_VERSION >= 22900
#include <shell/e-shell-view.h>
#include <shell/e-shell-searchbar.h>

static void
action_search_cb (EShellView *shell,
			GtkWidget *sb)
{
//	g_signal_connect_swapped (
//		search_bar, "changed",
//		G_CALLBACK (em_format_redraw), priv->html_display);
		g_print("my search\n");
//	g_print("search:%s\n", e_shell_searchbar_get_search_text(sb));
}

void
rss_search_bar_hook(void)
{
	EShellContent *shell_content;
	EMailReader *reader;
	GtkAction *action;
	EShellSearchbar *search_bar;
	gchar *action_name;
	EShellView *shell_view;
	EShellWindow *shell_window;

	CamelFolder *folder;
	EMFolderTree *folder_tree;
	EShellSidebar *shell_sidebar = e_shell_view_get_shell_sidebar(
					rss_shell_view);


	shell_content = e_shell_view_get_shell_content (rss_shell_view);
	reader = E_MAIL_READER (shell_content);

//	action_name = "mail-find";
//	action = e_mail_reader_get_action (reader, action_name);
//	g_signal_connect_swapped (
//		action, "activate",
//		G_CALLBACK (action_search_cb), reader);
	search_bar = e_shell_view_get_searchbar(rss_shell_view);
	//search_bar = e_shell_content_get_searchbar(shell_content);
	shell_view = e_shell_searchbar_get_shell_view (search_bar);
//	g_signal_connect_after (
//		"execute-search", "activate",
//		G_CALLBACK (action_search_cb), search_bar);
	shell_window = e_shell_view_get_shell_window (rss_shell_view);
	g_signal_connect_after (
			search_bar,
			"expose-event",
		G_CALLBACK (action_search_cb),
		rss_shell_view);
	e_shell_view_clear_search(shell_view);

//	g_signal_connect (
//		search_bar, "execute-search",
//		G_CALLBACK (action_search_cb), search_bar);

}
#endif
#endif

#endif

#ifdef HAVE_GECKO
void gecko_net_start(GtkMozEmbed *mozembed, GtkWidget *data);

void
gecko_net_start(GtkMozEmbed *mozembed, GtkWidget *data)
{
	gtk_widget_set_sensitive(data, TRUE);
}

void gecko_net_stop(GtkMozEmbed *mozembed, GtkWidget *data);

void
gecko_net_stop(GtkMozEmbed *mozembed, GtkWidget *data)
{
	gtk_widget_set_sensitive(data, FALSE);
}

gboolean gecko_over_link(GtkMozEmbed *mozembed);

gboolean
gecko_over_link(GtkMozEmbed *mozembed)
{
	gchar *link = gtk_moz_embed_get_link_message(mozembed);
	if (link && strlen(link)) {
		gchar *msg = g_strdup_printf("%s %s", _("Click to open"), link);
		g_free(link);
		taskbar_push_message(msg);
		g_free(msg);
	} else
		taskbar_pop_message();
	return FALSE;
}

gboolean gecko_click(GtkMozEmbed *mozembed, gpointer dom_event, gpointer user_data);

gboolean
gecko_click(GtkMozEmbed *mozembed, gpointer dom_event, gpointer user_data)
{
	gint button;
	GtkMenu *menu;
	gchar *link = NULL;
#if EVOLUTION_VERSION < 22900
	EMPopup *emp = NULL;
	GSList *menus = NULL;
	gint i=0, menu_size;
#endif

	if (-1 == (button = gecko_get_mouse_event_button (dom_event))) {
		g_warning ("Cannot determine mouse button!\n");
		return FALSE;
	}

	link = gtk_moz_embed_get_link_message((GtkMozEmbed *)rf->mozembed);

#if EVOLUTION_VERSION >= 22900
	menu = e_popup_menu_create_with_domain (
			rss_menu_items,
			0,
			(guint32)(strlen(link) ? 3:4),
			link,
			GETTEXT_PACKAGE);
	if (button == 2)
		gtk_menu_popup (
			GTK_MENU (menu),
			NULL, NULL, NULL, NULL,
			0, GDK_CURRENT_TIME);
#else
	emp = em_popup_new("org.gnome.evolution.mail.formathtmldisplay.popup");
	menu_size=sizeof(rss_menu_items)/sizeof(rss_menu_items[0]);
	if (strlen(link))
		i+=9;
	else
		menu_size-=2;

	for (; i<menu_size; i++)
		menus = g_slist_prepend(menus, &rss_menu_items[i]);

	e_popup_add_items((EPopup *)emp, menus, NULL, rss_menu_items_free, link);
	menu = e_popup_create_menu_once((EPopup *)emp, NULL, 0);

	if (button == 2)
		gtk_menu_popup(
			menu,
			NULL, NULL, NULL, NULL,
			button,
			gtk_get_current_event_time());
#endif

	/*normal click let event pass normally*/
	if (button == 0)
		gtk_moz_embed_load_url((GtkMozEmbed *)rf->mozembed, link);
	return FALSE;
}
#endif

gboolean
show_webkit(GtkWidget *webkit)
{
	gtk_widget_show_all(webkit);
	return FALSE;
}

#ifdef HAVE_RENDERKIT
static gboolean
org_gnome_rss_browser (EMFormatHTML *efh, void *eb, EMFormatHTMLPObject *pobject)
{
	struct _org_gnome_rss_controls_pobject *po =
			(struct _org_gnome_rss_controls_pobject *) pobject;
	EMFormat *myf = (EMFormat *)efh;
	gint width, height;
	GtkAdjustment *adj;
	gboolean online;
	guint engine = fallback_engine();

#ifdef HAVE_WEBKIT
	if (engine == 1) {
		rf->mozembed = (GtkWidget *)webkit_web_view_new();
		webkit_set_preferences();
		g_signal_connect (
			rf->mozembed,
			"populate-popup",
			G_CALLBACK (webkit_click),
			NULL);
		g_signal_connect (
			rf->mozembed,
			"hovering-over-link",
			G_CALLBACK (webkit_over_link),
			NULL);

//add zoom level
#if (WEBKIT_VERSION >= 1001007)
		g_signal_connect (
			rf->mozembed,
			"notify::load-status",
			G_CALLBACK(webkit_net_status),
			po->stopbut);
#endif
	}
#if (WEBKIT_VERSION >= 1001007)
		g_signal_connect (
			rf->mozembed,
			"notify::load-status",
			G_CALLBACK(webkit_history_status),
			po);
#endif
#endif

#ifdef HAVE_GECKO
	if (engine == 2) {
		rss_mozilla_init();	//in case we fail this is a failover
		rf->mozembed = gtk_moz_embed_new();
		d("mozembed=%p at %s:%d\n", rf->mozembed, __FILE__, __LINE__);
		gecko_set_preferences();

		/* FIXME add all those profile shits */
		gtk_scrolled_window_add_with_viewport(
				GTK_SCROLLED_WINDOW(moz),
				GTK_WIDGET(rf->mozembed));
		gtk_scrolled_window_set_shadow_type (
				GTK_SCROLLED_WINDOW(moz),
				GTK_SHADOW_ETCHED_IN);
		g_signal_connect (rf->mozembed,
				"dom_mouse_click",
				G_CALLBACK(gecko_click),
				moz);
		g_signal_connect (
				rf->mozembed,
				"link_message",
				G_CALLBACK(gecko_over_link),
				moz);
		g_signal_connect (
				rf->mozembed,
				"net_start",
				G_CALLBACK(gecko_net_start),
				po->stopbut);
		g_signal_connect (
				rf->mozembed,
				"net_stop",
				G_CALLBACK(gecko_net_stop),
				po->stopbut);
	}
#endif

#if (DATASERVER_VERSION >= 2031002)
	online =  camel_session_get_online (session);
#else
	online =  camel_session_is_online (session);
#endif

#ifdef HAVE_WEBKIT
	if (engine == 1) {
		d("Render engine Webkit\n");
		if (!online)
			webkit_web_view_open(
				WEBKIT_WEB_VIEW(rf->mozembed),
				"about:blank");
	}
#endif

#ifdef HAVE_GECKO
	if (engine == 2) {
		d("Render engine Gecko\n");
		if (!online) {
			gtk_moz_embed_stop_load(
				(GtkMozEmbed *)rf->mozembed);
			gtk_moz_embed_load_url (
				(GtkMozEmbed *)rf->mozembed, "about:blank");
		}
	}
#endif


	//aparently webkit will hang if its not run from the main thread when compiled
	//with soup
	//this is as ugly as can be
	if (engine == 1)
		g_idle_add((GSourceFunc)show_webkit, rf->mozembed);
	if (engine == 2)
		gtk_widget_show_all(rf->mozembed);

	gtk_container_add ((GtkContainer *) eb, rf->mozembed);
	//appears distroying webkit's parent window results in crash
	g_object_ref(eb);
	po->container = eb;
	rf->headers_mode = myf->mode;

#if EVOLUTION_VERSION >= 23103
	po->html = (GtkWidget *)em_format_html_get_web_view (efh);
#else
	po->html = GTK_WIDGET(efh->html);
#endif

	adj = gtk_scrolled_window_get_vadjustment(
		(GtkScrolledWindow *)gtk_widget_get_parent(po->html));
#if GTK_VERSION >= 2014000
	height = (int)gtk_adjustment_get_page_size(adj);
#else
	height = (int)(adj->page_size);
#endif
	adj = gtk_scrolled_window_get_hadjustment(
		(GtkScrolledWindow *)gtk_widget_get_parent(po->html));
#if GTK_VERSION >= 2014000
	width = (int)gtk_adjustment_get_page_size(adj);
#else
	width = (int)(adj->page_size);
#endif
	gtk_widget_set_size_request(rf->mozembed, width-32, height);
	po->sh_handler = g_signal_connect(adj,
		"changed",
		G_CALLBACK(rss_browser_set_hsize),
		NULL);
	po->chandler = g_signal_connect(rf->mozembed,//efh->html,
		"size_allocate",
		G_CALLBACK(rss_browser_update_content),
		po);
	return TRUE;
}
#endif

static gboolean
org_gnome_rss_rfrcomm (EMFormatHTML *efh, void *eb,
			EMFormatHTMLPObject *pobject)
{
	struct _org_gnome_rss_controls_pobject *po =
			(struct _org_gnome_rss_controls_pobject *) pobject;
	GtkWidget *hbox = gtk_hbox_new (FALSE, 0);
	GtkWidget *button;

	gchar *mem = g_strdup_printf("%s(%d):",  _("Comments"), po->counter);
	GtkWidget *label = gtk_link_button_new_with_label(po->website, mem);
	gtk_widget_show (label);
	g_free(mem);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	button = gtk_button_new_with_label(_("Refresh"));
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_widget_show(hbox);
	g_signal_connect (button, "clicked", G_CALLBACK(refresh_cb), efh);
	if (GTK_IS_WIDGET(eb))
		gtk_container_add ((GtkContainer *) eb, hbox);
	return TRUE;
}

static gboolean
org_gnome_rss_controls (EMFormatHTML *efh, void *eb, EMFormatHTMLPObject *pobject)
{
	struct _org_gnome_rss_controls_pobject *po =
			(struct _org_gnome_rss_controls_pobject *) pobject;
	GtkWidget *vbox = gtk_vbox_new (TRUE, 1);
	GtkWidget *hbox2 = gtk_hbox_new (FALSE, 0);
	GtkWidget *label3 = gtk_label_new ("");
	GtkWidget *button, *button2, *button3, *button4, *button5;
	gboolean online;

	gchar *mem = g_strdup_printf(" <b>%s: </b>", _("Feed view"));
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label3), mem);
	gtk_widget_show (label3);
	gtk_box_pack_start (GTK_BOX (hbox2), label3, TRUE, TRUE, 0);

	button = gtk_button_new_with_label(
				rf->cur_format ? _("Show Summary") :
						_("Show Full Text"));

	gtk_button_set_image (
		GTK_BUTTON (button),
		gtk_image_new_from_icon_name (
			rf->cur_format ? "text-x-generic" : "text-html",
			GTK_ICON_SIZE_BUTTON));

	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_HALF);
	g_signal_connect (button, "clicked", G_CALLBACK(summary_cb), efh);
	gtk_box_pack_start (GTK_BOX (hbox2), button, TRUE, TRUE, 0);
	gtk_widget_show_all (button);

#if (DATASERVER_VERSION >= 2031002)
	online =  camel_session_get_online (session);
#else
	online =  camel_session_is_online (session);
#endif

	if (rf->cur_format) {
		button4 = po->backbut;
		g_signal_connect (
			button4,
			"clicked",
			G_CALLBACK(back_cb),
			efh);
		gtk_button_set_relief(GTK_BUTTON(button4), GTK_RELIEF_HALF);
		gtk_widget_set_sensitive (button4, online);
		gtk_widget_show (button4);
		gtk_box_pack_start (
			GTK_BOX (hbox2),
			button4,
			TRUE, TRUE, 0);
		button5 = po->forwbut;
		g_signal_connect (
			button5,
			"clicked",
			G_CALLBACK(forward_cb),
			efh);
		gtk_button_set_relief(GTK_BUTTON(button5), GTK_RELIEF_HALF);
		gtk_widget_set_sensitive (button5, online);
		gtk_widget_show (button5);
		gtk_box_pack_start (GTK_BOX (hbox2), button5, TRUE, TRUE, 0);
		button2 = po->stopbut;
		g_signal_connect (
			button2,
			"clicked",
			G_CALLBACK(stop_cb),
			efh);
		gtk_button_set_relief(GTK_BUTTON(button2), GTK_RELIEF_HALF);
		gtk_widget_set_sensitive (button2, online);
		gtk_widget_show (button2);
		gtk_box_pack_start (GTK_BOX (hbox2), button2, TRUE, TRUE, 0);
		button3 = gtk_button_new_from_stock (GTK_STOCK_REFRESH);
		g_signal_connect (
			button3,
			"clicked",
			G_CALLBACK(reload_cb),
			po->website);
		gtk_button_set_relief(GTK_BUTTON(button3), GTK_RELIEF_HALF);
		gtk_widget_set_sensitive (button3, online);
		gtk_widget_show (button3);
		gtk_box_pack_start (GTK_BOX (hbox2), button3, TRUE, TRUE, 0);
	}
	gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0);
	gtk_widget_show_all (vbox);

//		int width = vbox->allocation.width;
//		int height = vbox->allocation.height;

	if (GTK_IS_WIDGET(eb))
		gtk_container_add ((GtkContainer *) eb, vbox);
//	GtkHTMLEmbedded *myeb = eb;
//	gtk_widget_size_request(myeb->widget, &req);
//	g_print("BOX ww:%d,hh%d\n", myeb->width, myeb->height);
//	g_print("BOX ww:%d,hh%d\n", width, height);

	po->html = vbox;
	po->mem = mem;

	return TRUE;
}

void
cancel_comments_session(SoupSession *sess)
{
	g_print("comment session to cancel:%p\n", sess);
	soup_session_abort(sess);
}

void free_rss_controls(EMFormatHTMLPObject *o);

void
free_rss_controls(EMFormatHTMLPObject *o)
{
	struct _org_gnome_rss_controls_pobject *po =
			(struct _org_gnome_rss_controls_pobject *) o;
	if (po->mem)
		g_free(po->mem);
	if (po->website)
		g_free(po->website);
	//gtk_widget_destroy(po->html);
//	g_slist_foreach(comments_session, (GFunc)cancel_comments_session, NULL);
//	g_slist_free(comments_session);
//	comments_session = NULL;
}

void free_rss_browser(EMFormatHTMLPObject *o);

void
free_rss_browser(EMFormatHTMLPObject *o)
{
	struct _org_gnome_rss_controls_pobject *po =
			(struct _org_gnome_rss_controls_pobject *) o;
	gpointer key = g_hash_table_lookup(rf->key_session, po->website);
	guint engine;
	GtkAdjustment *adj;
#if EVOLUTION_VERSION >= 23103
	EWebView *web_view;
#endif

	d("key sess:%p\n", key);
	if (key) {
		g_hash_table_remove(rf->key_session, po->website);
		soup_session_abort(key);
	}
	engine = gconf_client_get_int(rss_gconf,
			GCONF_KEY_HTML_RENDER,
			NULL);
#ifdef HAVE_GECKO
	if (engine == 2) {
		gtk_moz_embed_stop_load((GtkMozEmbed *)rf->mozembed);
	}
#endif
	if (rf->mozembed) {
//		if (engine == 2) //crashes webkit - https://bugs.webkit.org/show_bug.cgi?id=25042
			gtk_widget_destroy(rf->mozembed);
		rf->mozembed = NULL;
	}

#if EVOLUTION_VERSION >= 23103
	web_view = em_format_html_get_web_view (po->format);
	adj = gtk_scrolled_window_get_hadjustment(
		(GtkScrolledWindow *)gtk_widget_get_parent(
					GTK_WIDGET(web_view)));
#else
	adj = gtk_scrolled_window_get_hadjustment(
		(GtkScrolledWindow *)gtk_widget_get_parent(
					GTK_WIDGET(po->format->html)));
#endif
	g_signal_handler_disconnect(adj, po->sh_handler);
	g_object_unref(po->container);
	g_free(po->website);
	browser_fetching = 0;
}


void org_gnome_evolution_presend (EPlugin *ep, EMEventTargetComposer *t);

void
org_gnome_evolution_presend (EPlugin *ep, EMEventTargetComposer *t)
{
#ifdef HAVE_GTKHTMLEDITOR
	xmlChar *buff = NULL;
	xmlDoc *doc;
	gint size;
	gsize length;
	gchar *text;

	/* unfortunately e_msg_composer does not have raw get/set text body
	 * so it is far easier using gtkhtml_editor_* functions rather than
	 * procesing CamelMimeMessage or GByteArray
	 */
	text = gtkhtml_editor_get_text_html ((GtkhtmlEditor *)t->composer, &length);

	doc = rss_html_url_decode(text, strlen(text));
	if (doc) {
		htmlDocDumpMemory(doc, &buff, &size);
		xmlFree(doc);
	}

	gtkhtml_editor_set_text_html((GtkhtmlEditor *)t->composer, (gchar *)buff, size);
#endif
}

void org_gnome_cooly_format_rss(void *ep, EMFormatHookTarget *t);

void org_gnome_cooly_format_rss(void *ep, EMFormatHookTarget *t)	//camelmimepart
{
	GError *err = NULL;
	GString *content;
	xmlChar *buff = NULL;
	CamelContentType *type;
	gchar *feedid = NULL;
	gchar *comments = NULL;
	gchar *category = NULL;
	GdkPixbuf *pixbuf = NULL;
	guint engine = 0;
	int size;
	CamelDataWrapper *dw = camel_data_wrapper_new();
	CamelMimePart *part = camel_mime_part_new();
	CamelMimePart *message;
	CamelStream *fstream = camel_stream_mem_new();
	CamelStreamMem *stream = (CamelStreamMem *)camel_stream_mem_new();
	CamelDataWrapper *mcontent;
	EMFormatHTML *emfh = (EMFormatHTML *)t->format;
	gchar *rfrclsid, *addr, *subject, *result;
	const char *website;
	struct _org_gnome_rss_controls_pobject *pobj;
	gpointer is_html;
	gchar *classid, *tmp;
	GtkWidget *button2, *button3, *button4;
	xmlDoc *src;
	xmlChar *wid;
	GByteArray *buffer;
	GdkPixbuf *pix;
	gchar *feed_dir, *feed_file, *iconfile;
	gchar *tmp_path, *tmp_file;
#if EVOLUTION_VERSION >= 22900 //kb//
	GdkColor color;
#endif
	GtkAllocation alloc;

	current_pobject = t->format;
	d("Formatting...\n");

	message = CAMEL_IS_MIME_MESSAGE(t->part) ?
			t->part :
			(CamelMimePart *)t->format->message;

	/* force loading of images even if mail images disabled */
	/* //KB// */
	//emfh->load_http_now = TRUE;
	/* assuming 0xffffff will ruin dark themes */
#if EVOLUTION_VERSION < 22900 //kb//
	frame_colour = emfh->frame_colour;// ? emfh->frame_colour: 0xffffff;
	content_colour = emfh->content_colour;// ? emfh->content_colour: 0xffffff;
	text_colour = emfh->text_colour;// ? emfh->text_colour: 0xffffff;
#else

	em_format_html_get_color(
				emfh,
				EM_FORMAT_HTML_COLOR_FRAME,
				&color);
	frame_colour = e_color_to_value(&color);

	em_format_html_get_color(
				emfh,
				EM_FORMAT_HTML_COLOR_CONTENT,
				&color);
	content_colour = e_color_to_value(&color);

	em_format_html_get_color(
				emfh,
				EM_FORMAT_HTML_COLOR_TEXT,
				&color);
	text_colour = e_color_to_value(&color);
#endif

	type = camel_mime_part_get_content_type(message);
	website = camel_medium_get_header (
			CAMEL_MEDIUM (message), "Website");
	if (!website)
		goto fmerror;
	addr = (gchar *)camel_header_location_decode(website);
	feedid  = (gchar *)camel_medium_get_header(
			CAMEL_MEDIUM(message), "RSS-ID");
	comments  = (gchar *)camel_medium_get_header (
				CAMEL_MEDIUM(message),
				"X-Evolution-rss-comments");
	category  = (gchar *)camel_medium_get_header(
				CAMEL_MEDIUM(message),
				"X-Evolution-rss-category");
	subject = camel_header_decode_string(
			camel_medium_get_header (CAMEL_MEDIUM (message),
			"Subject"), NULL);

	is_html = NULL;
	if (feedid)
		is_html =  g_hash_table_lookup(
				rf->hrh,
				g_strstrip(feedid)); //feedid is modified
	if (comments)
		comments = g_strstrip(comments);

	if (!rf->chg_format)
		rf->cur_format = GPOINTER_TO_INT(is_html);

	if (rf->chg_format)
		rf->chg_format = 0;

	classid = g_strdup_printf ("org-gnome-rss-controls-%d",
			org_gnome_rss_controls_counter_id);
	org_gnome_rss_controls_counter_id++;
	pobj = (struct _org_gnome_rss_controls_pobject *)
		em_format_html_add_pobject (
			(EMFormatHTML *) t->format,
			sizeof(*pobj),
			classid,
			message,
			(EMFormatHTMLPObjectFunc)org_gnome_rss_controls);
	pobj->is_html = GPOINTER_TO_INT(is_html);
	pobj->website = g_strstrip(g_strdup((gchar *)website));
	pobj->stream = t->stream;
	pobj->object.free = free_rss_controls;
	camel_stream_printf (
		t->stream,
		"<object classid=%s></object>\n",
		classid);
	//pre-create stop button as we need it to control it later
	button3 = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
	pobj->backbut = button3;
	button4 = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
	pobj->forwbut = button4;
	button2 = gtk_button_new_from_stock (GTK_STOCK_STOP);
	pobj->stopbut = button2;
	g_free (classid);


	if (rf->cur_format || (feedid && is_html && rf->cur_format)) {
		engine = fallback_engine();
#ifdef HAVE_RENDERKIT
		if (engine && engine != 10) {
			char *classid = g_strdup_printf(
					"org-gnome-rss-controls-%d",
					org_gnome_rss_controls_counter_id);
			org_gnome_rss_controls_counter_id++;
			pobj = (struct _org_gnome_rss_controls_pobject *)
				em_format_html_add_pobject(
					(EMFormatHTML *) t->format,
					sizeof(*pobj),
					classid,
					message,
					(EMFormatHTMLPObjectFunc)org_gnome_rss_browser);
			pobj->website = g_strstrip(g_strdup((gchar *)website));
			pobj->is_html = GPOINTER_TO_INT(is_html);
			pobj->format = (EMFormatHTML *)t->format;
			pobj->object.free = free_rss_browser;
			pobj->part = t->part;
			pobj->stopbut =  button2;
			pobj->backbut = button3;
			pobj->forwbut = button4;
//			camel_stream_printf (t->stream,
//				"<div style=\"border: solid #%06x 1px; background-color: #%06x; color: #%06x;\">\n",
//				frame_colour & 0xffffff,
//				content_colour & 0xffffff,
//				text_colour & 0xffffff);
			camel_stream_printf(t->stream,
				"<table style=\"border: solid #%06x 1px; background-color: #%06x; color: #%06x;\" cellpadding=1 cellspacing=0><tr><td align=center>",
				frame_colour & 0xffffff,
				frame_colour & 0xffffff,
				text_colour & 0xffffff);
			camel_stream_printf (t->stream,
				"<object classid=%s></object></td></tr></table>",//</div>\n",
				classid);
			g_free (classid);
			goto out;
		}
#endif
		//replace with unblocking
		content = fetch_blocking(addr, NULL, NULL, textcb, NULL, &err);
		if (err) {
			//we do not need to setup a pop error menu since we're in
			//formatting process. But instead display mail body an error
			//such proxy error or transport error
			camel_stream_printf (t->stream,
				"<div style=\"border: solid #%06x 1px; background-color: #%06x; color: #%06x;\">\n",
				frame_colour & 0xffffff,
				content_colour & 0xffffff,
				text_colour & 0xffffff);
			camel_stream_printf(t->stream,
				"<div style=\"border: solid 0px; padding: 4px;\">\n");
			camel_stream_printf (t->stream, "<h3>Error!</h3>");
			camel_stream_printf (t->stream, "%s", err->message);
			camel_stream_printf (t->stream, "</div>");
			goto out;
		}

		tmp = decode_utf8_entities(content->str);
		src = (xmlDoc *)parse_html(addr, tmp, strlen(tmp));

		if (src) {
			htmlDocDumpMemory(src, &buff, &size);
			d("htmlDocDumpMemory:%s\n", buff);
			xmlFree(src);
		} else
			goto out;

		camel_stream_printf (fstream,
			"<div style=\"border: solid #%06x 1px; background-color: #%06x; color: #%06x;\">\n",
			frame_colour & 0xffffff,
			content_colour & 0xffffff,
			text_colour & 0xffffff);
		camel_stream_printf (fstream,
			"<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
			"<b><font size=+1><a href=%s>%s</a></font></b></div>",
			content_colour & 0xEDECEB & 0xffffff,
			text_colour & 0xffffff,
			website, subject);
		if (category)
			camel_stream_printf(fstream,
				"<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
				"<b><font size=-1>%s: %s</font></b></div>",
				content_colour & 0xEDECEB & 0xffffff, text_colour & 0xffffff,
				_("Posted under"), category);
		camel_stream_printf (fstream,
				"<div style=\"border: solid #%06x 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
				"%s</div>",
				frame_colour & 0xffffff,
				content_colour & 0xffffff,
				text_colour & 0xffffff,
				buff);

		g_string_free(content, 1);
	} else {
		gchar *wids;
		xmlDoc *src;
		guint width;
		GtkWidget *obj;
#if EVOLUTION_VERSION >= 23103
		EWebView *web_view;
		web_view = em_format_html_get_web_view (emfh);
		obj = (GtkWidget *)web_view;
#else
		obj = (GtkWidget *)emfh->html;
#endif

		d("normal html rendering\n");
		buffer = g_byte_array_new ();
		camel_stream_mem_set_byte_array (stream, buffer);
#if EVOLUTION_VERSION >= 23100
		mcontent = camel_medium_get_content(
				CAMEL_MEDIUM(t->part));
#else
		mcontent = camel_medium_get_content_object(
				CAMEL_MEDIUM(t->part));
#endif
		camel_data_wrapper_write_to_stream(
			mcontent,
			(CamelStream *)stream);
		g_byte_array_append (buffer, (unsigned char *)"", 1);
//#ifdef EVOLUTION_2_12	//aparently this ("?" char parsing) is fixed in 2.12
//		//then again this does not work in evo > 2.12 perhaps is gtkhtml related
//		buff = buffer->data;
//#else
		if (camel_content_type_is(type, "text", "evolution-rss-feed")) {	//old evolution-rss content type
			tmp = decode_utf8_entities((gchar *)(buffer->data));
		} else
			tmp = g_strdup((gchar *)(buffer->data));

#if GTK_VERSION >= 2018000
		gtk_widget_get_allocation(obj, &alloc);
#else
		alloc.width = obj->allocation.width;;
#endif
		width = alloc.width - 56;
		wids = g_strdup_printf("%d", width);
		src = (xmlDoc *)parse_html_sux(
				tmp,
				strlen(tmp));
		if (src) {
			xmlNode *doc = (xmlNode *)src;
			while ((doc = html_find(doc, (gchar *)"img"))) {
				int real_width = 0;
				xmlChar *url = xmlGetProp(
						doc,
						(xmlChar *)"src");
				gchar *real_image = verify_image(
							(gchar *)url,
							emfh);
				if (real_image) {
					xmlSetProp(
						doc,
						(xmlChar *)"src",
						(xmlChar *)real_image);
				}
				if (gconf_client_get_bool (rss_gconf,
					GCONF_KEY_IMAGE_RESIZE, NULL) && real_image) {
					pix = gdk_pixbuf_new_from_file(
						(const char *)real_image+7, //skip scheme part
						(GError **)NULL);
					if (pix)
						real_width = gdk_pixbuf_get_width(pix);

					d("real_image:%s\n", real_image);
					d("width:%d\n", width);
					d("real_width:%d\n", real_width);

					wid = xmlGetProp(
						doc,
						(xmlChar *)"width");
					if (wid) {
						if (atof((const char *)wid) > width)
							xmlSetProp(
								doc,
								(xmlChar *)"width",
								(xmlChar *)wids);
						g_free(wid);
						goto pixdone;
					} else if (real_width > width) {
						xmlSetProp(
							doc,
							(xmlChar *)"width",
							(xmlChar *)wids);
					}
pixdone:			g_free(real_image);
				}
			}
			xmlDocDumpMemory(src, &buff, (int*)&size);
			xmlFree(src);
		}
		g_free(wids);

		g_byte_array_free (buffer, 1);
#if (DATASERVER_VERSION >= 2031001)
		g_object_unref(stream);
#else
		camel_object_unref(stream);
#endif
	//	char *buff = decode_html_entities(buffer2);
///		buff=tmp;

		feed_dir = rss_component_peek_base_directory();
		tmp_file = g_strconcat(feedid, ".img", NULL);
		tmp_path = g_build_path(G_DIR_SEPARATOR_S,
				feed_dir, tmp_file, NULL);
		g_free(tmp_file);
		g_free(feed_dir);
#if EVOLUTION_VERSION >= 23000
		feed_file = g_filename_to_uri(tmp_path, NULL, NULL);
#else
		feed_file = g_strdup(tmp_path);
#endif

		camel_stream_printf (
			fstream,
			"<div style=\"border: solid #%06x 1px; background-color: #%06x; padding: 2px; color: #%06x;\">",
			frame_colour & 0xffffff,
			content_colour & 0xEDECEB & 0xffffff,
			text_colour & 0xffffff);
		if (g_file_test(tmp_path, G_FILE_TEST_EXISTS)){
			if ((pixbuf = gdk_pixbuf_new_from_file(tmp_path, NULL))) {
				camel_stream_printf (fstream,
					"<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
					"<img height=16 src=%s>"
					"<b><font size=+1><a href=%s>%s</a></font></b></div>",
					content_colour & 0xEDECEB & 0xffffff,
					text_colour & 0xffffff,
					feed_file,
					website,
					subject);
				g_object_unref(pixbuf);
				g_free(feed_file);
				goto render_body;
			}
		g_free(tmp_path);
		}
		tmp_file = g_build_filename (EVOLUTION_ICONDIR,
				"rss-16.png",
				NULL);
#if EVOLUTION_VERSION >= 23000
		iconfile = g_filename_to_uri(tmp_file, NULL, NULL);
#else
		iconfile = g_strdup(tmp_file);
#endif
		g_free(tmp_file);
		camel_stream_printf (fstream,
			"<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
			"<img height=16 src=%s>"
			"<b><font size=+1><a href=%s>%s</a></font></b></div>",
			content_colour & 0xEDECEB & 0xffffff, text_colour & 0xffffff,
			iconfile, website, subject);
		g_free(iconfile);
		g_free(feed_file);

render_body:	if (category)
			camel_stream_printf(fstream,
				"<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
				"<b><font size=-1>Posted under: %s</font></b></div>",
				content_colour & 0xEDECEB & 0xffffff,
				text_colour & 0xffffff,
				category);
		camel_stream_printf (fstream,
				"<div style=\"border: solid #%06x 0px; background-color: #%06x; padding: 10px; color: #%06x;\">"
				"%s</div>",
				frame_colour & 0xffffff,
				content_colour & 0xffffff,
				text_colour & 0xffffff,
				buff);
		if (comments && gconf_client_get_bool (rss_gconf,
						GCONF_KEY_SHOW_COMMENTS,
						NULL)) {
			if (commstream) {
				camel_stream_printf (fstream,
					"<div style=\"border: solid #%06x 0px; background-color: #%06x; padding: 2px; color: #%06x;\">",
					frame_colour & 0xffffff,
					content_colour & 0xEDECEB & 0xffffff,
					text_colour & 0xffffff);
				result = print_comments(comments, commstream, (EMFormatHTML *)t->format);
				g_free(commstream);
				rfrclsid = g_strdup_printf ("org-gnome-rss-controls-%d",
					org_gnome_rss_controls_counter_id);
				org_gnome_rss_controls_counter_id++;
				pobj = (struct _org_gnome_rss_controls_pobject *)
							em_format_html_add_pobject ((EMFormatHTML *) t->format,
									sizeof(*pobj),
									rfrclsid,
									message,
									(EMFormatHTMLPObjectFunc)org_gnome_rss_rfrcomm);
				pobj->counter = commcnt;
				pobj->website = g_strdup(comments);
				//this might not be needed but we want to make sure po->html is destroyed
				pobj->object.free = free_rss_controls;
				camel_stream_printf(fstream,
					"<object height=25 classid=%s></object>", rfrclsid);
				if (result && strlen(result)) {
					camel_stream_printf(fstream,
					"<div style=\"border: solid #%06x 0px; background-color: #%06x; padding: 10px; color: #%06x;\">%s",
						frame_colour & 0xffffff,
						content_colour & 0xffffff,
						text_colour & 0xffffff,
						result);
					g_free(result);
				}
				g_free(rfrclsid);
				commstream = NULL;
			} else {
				fetch_comments(comments, (EMFormatHTML *)t->format);
			}
			camel_stream_printf (fstream, "</div>");
		}
		camel_stream_printf (fstream, "</div>");
	}

	//this is required for proper charset rendering when html
	camel_data_wrapper_construct_from_stream(dw, fstream);
#if EVOLUTION_VERSION >= 23100
	camel_medium_set_content((CamelMedium *)part, dw);
#else
	camel_medium_set_content_object((CamelMedium *)part, dw);
#endif
	em_format_format_text((EMFormat *)t->format,
				(CamelStream *)t->stream,
				(CamelDataWrapper *)part);
#if (DATASERVER_VERSION >= 2031001)
	g_object_unref(dw);
	g_object_unref(part);
	g_object_unref(fstream);
#else
	camel_object_unref(dw);
	camel_object_unref(part);
	camel_object_unref(fstream);
#endif
	g_free(buff);
	g_free(subject);

out:	if (addr)
		g_free(addr);
	return;
fmerror:
	camel_stream_printf (t->stream,
		"<div style=\"border: solid #%06x 1px; background-color: #%06x; color: #%06x;\">\n",
		frame_colour & 0xffffff, content_colour & 0xffffff, text_colour & 0xffffff);
	camel_stream_printf(t->stream,
		"<div style=\"border: solid 0px; padding: 4px;\">\n");
	camel_stream_printf (t->stream,
		"<h3>Formatting error!</h3>"
		"Feed article corrupted! Cannot format article.");
	camel_stream_printf (t->stream,
		"</div></div>");
	return;
}

#if EVOLUTION_VERSION < 22900 //kb//
void org_gnome_cooly_folder_refresh(void *ep, EMEventTargetFolder *t);
#else
void org_gnome_cooly_folder_refresh(void *ep, EShellView *shell_view);
#endif

#if EVOLUTION_VERSION < 22900 //kb//
void org_gnome_cooly_folder_refresh(void *ep, EMEventTargetFolder *t)
#else
void org_gnome_cooly_folder_refresh(void *ep, EShellView *shell_view)
#endif
{
	gchar *folder_name;
	gchar *main_folder = get_main_folder();
	gchar *ofolder, *name, *fname, *key, *rss_folder;
	gboolean online;
#if EVOLUTION_VERSION > 22900 //kb//
	CamelFolder *folder;
	EMFolderTree *folder_tree;
	EShellSidebar *shell_sidebar = e_shell_view_get_shell_sidebar(
					shell_view);
	EActivity *taskid;

	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);
	folder = em_folder_tree_get_selected_folder (folder_tree);
	g_return_if_fail (folder != NULL);
#if (DATASERVER_VERSION >= 2031001)
	folder_name = (gchar *)camel_folder_get_full_name(folder);
#else
	folder_name = folder->full_name;
#endif
#else
	guint taskid;
	folder_name = t->uri;
#endif
	if (folder_name == NULL
	  || g_ascii_strncasecmp(folder_name, main_folder, strlen(main_folder)))
		goto out;
	if (!g_ascii_strcasecmp(folder_name, main_folder))
		goto out;
	rss_folder = extract_main_folder(folder_name);
	if (!rss_folder)
		goto out;
	ofolder = g_hash_table_lookup(rf->feed_folders, rss_folder);
	fname = ofolder ? ofolder : rss_folder;
	key = g_hash_table_lookup(rf->hrname, fname);
	if (!key)
		goto out;
	name = g_strdup_printf(
		"%s: %s",
		_("Fetching feed"),
		(gchar *)g_hash_table_lookup(rf->hrname_r, key));

#if (DATASERVER_VERSION >= 2031002)
	online =  camel_session_get_online (session);
#else
	online =  camel_session_is_online (session);
#endif

	if (g_hash_table_lookup(rf->hre, key)
	  && !rf->pending && !rf->feed_queue
	  && !single_pending && online) {
		single_pending = TRUE;
		check_folders();
		rf->err = NULL;
		taskid = taskbar_op_message(name, key);
		network_timeout();
		if (!fetch_one_feed(fname, key, statuscb))
			taskbar_op_finish(key);
		single_pending = FALSE;
	}
	g_free(name);
out:	return;
}

#if (EVOLUTION_VERSION >= 22306)
void org_gnome_cooly_folder_icon(void *ep, EMEventTargetCustomIcon *t);

void org_gnome_cooly_folder_icon(void *ep, EMEventTargetCustomIcon *t)
{
	gchar *rss_folder, *ofolder, *key;
	gchar *main_folder = get_main_folder();
#if (EVOLUTION_VERSION < 22703)
	GdkPixbuf *icon, *pixbuf;
	gchar *iconfile;
#endif

	main_folder = get_main_folder();
	if (t->folder_name == NULL
	  || g_ascii_strncasecmp(t->folder_name, main_folder, strlen(main_folder)))
		goto out;
	if (!g_ascii_strcasecmp(t->folder_name, main_folder))
		goto normal;
	rss_folder = extract_main_folder((gchar *)t->folder_name);
	if (!rss_folder)
		goto out;
	if (!icons)
		icons = g_hash_table_new_full(
				g_str_hash, g_str_equal, g_free, NULL);
	ofolder = g_hash_table_lookup(rf->feed_folders, rss_folder);
	key = g_hash_table_lookup(rf->hrname,
				ofolder ? ofolder : rss_folder);
	g_free(rss_folder);
	if (!key)
		goto normal;


#if (EVOLUTION_VERSION >= 22703)
	if (!evolution_store)
		evolution_store = t->store;

	if (!(g_hash_table_lookup(icons, key))) {
#else
	if (!(icon = g_hash_table_lookup(icons, key))) {
#endif
		if (gconf_client_get_bool (rss_gconf, GCONF_KEY_FEED_ICON, NULL)) {
//			if (g_file_test(feed_file, G_FILE_TEST_EXISTS)) {
			// unfortunately e_icon_factory_get_icon return broken image in case of error
			// we use gdk_pixbuf_new_from_file to test the validity of the image file
#if (EVOLUTION_VERSION >= 22703)
			if (display_folder_icon(t->store, key))
				goto out;
#else
			gchar *feed_dir = rss_component_peek_base_directory();
			gchar *feed_file = g_strdup_printf(
						"%s" G_DIR_SEPARATOR_S "%s.img", feed_dir, key);
			pixbuf = gdk_pixbuf_new_from_file(feed_file, NULL);
			g_free(feed_dir);

			if (pixbuf) {
				icon = e_icon_factory_get_icon (feed_file, E_ICON_SIZE_MENU);
				g_hash_table_insert(icons, g_strdup(key), icon);
				g_object_set (t->renderer, "pixbuf", icon, "visible", 1, NULL);
			} else
				goto normal; //failed to load the icon so just throw the default

			g_free(feed_file);
#endif
		}
	} else {
#if (EVOLUTION_VERSION >= 22703)
		gtk_tree_store_set (
			t->store, t->iter,
			COL_STRING_ICON_NAME, key,
			-1);
#else
	g_object_set (t->renderer, "pixbuf", icon, "visible", 1, NULL);
#endif
		goto out;
	}

#if (EVOLUTION_VERSION >= 22703)
normal:
	//if (key)
		gtk_tree_store_set (
			t->store, t->iter,
			COL_STRING_ICON_NAME, "rss-16",
			-1);
#else
normal:	if (!initialised) { //move this to startup
		iconfile = g_build_filename (EVOLUTION_ICONDIR,
			"rss-16.png",
			NULL);
		folder_icon = e_icon_factory_get_icon (iconfile, E_ICON_SIZE_MENU);
		g_free(iconfile);
		initialised = TRUE;
	}
	g_object_set (t->renderer, "pixbuf", folder_icon, "visible", 1, NULL);
#endif
out:	g_free(main_folder);
	return;
}
#endif

#ifdef EVOLUTION_2_12
void org_gnome_evolution_rss_article_show(void *ep, EMEventTargetMessage *t);
#else
void org_gnome_evolution_rss_article_show(void *ep, void *t);
#endif

#ifdef EVOLUTION_2_12
void org_gnome_evolution_rss_article_show(void *ep, EMEventTargetMessage *t)
{
	if (rf && (!inhibit_read || !delete_op))
		rf->current_uid = g_strdup(t->uid);
}
#else
void org_gnome_evolution_rss_article_show(void *ep, void *t)
{
}
#endif

gboolean
check_chn_name(gchar *chn_name)
{
	return g_hash_table_lookup(rf->hrname, chn_name)
		? 1:0;
}

// recursively create new name by adding #number to the end of channel name
gchar *
generate_safe_chn_name(gchar *chn_name)
{
	guint i = 0;
	gchar *c;
	gchar *stmp, *tmp2 = NULL;
	tmp2 = g_strdup(chn_name);
	while (check_chn_name(tmp2)) {
		GString *result = g_string_new (NULL);
		gchar *tmp = tmp2;
		if ((c = strrchr(tmp, '#'))) {
			if (isdigit(*(c+1))) {
				stmp = g_strndup(tmp, c - tmp);
				while (isdigit(*(c+1))) {
					g_string_append_c(result, *(c+1));
					c++;
				}
				i = atoi(result->str);
				tmp2 = g_strdup_printf("%s#%d", stmp, i+1);
				g_free(stmp);
			} else
				tmp2 = g_strdup_printf("%s #%d", tmp, i+1);
		} else
			tmp2 = g_strdup_printf("%s #%d", tmp, i+1);
		memset(result->str, 0, result->len);
		g_string_free (result, TRUE);
		g_free(tmp);
	}
	return tmp2;
}

gchar *
search_rss(char *buffer, int len)
{
	gchar *app;
	xmlNode *doc = (xmlNode *)parse_html_sux (buffer, len);
	while (doc) {
		doc = html_find(doc, (gchar *)"link");
		app = (gchar *)xmlGetProp(doc, (xmlChar *)"type");
		if (app && (!g_ascii_strcasecmp(app, "application/atom+xml")
		|| !g_ascii_strcasecmp(app, "application/xml")
		|| !g_ascii_strcasecmp(app, "application/rss+xml"))) {
			return (gchar *)xmlGetProp(doc, (xmlChar *)"href");
		}
		xmlFree(app);
	}
	return NULL;
}

void
prepare_hashes(void)
{
	if (rf->hr == NULL)
		rf->hr  = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				g_free);
	if (rf->hre == NULL)
		rf->hre = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				NULL);
	if (rf->hrh == NULL)
		rf->hrh = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				NULL);
	if (rf->hrt == NULL)
		rf->hrt = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				g_free);
	if (rf->hruser == NULL)
		rf->hruser = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				g_free);
	if (rf->hrpass == NULL)
		rf->hrpass = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				g_free);
	if (rf->hrname == NULL)
		rf->hrname = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				NULL);
	if (rf->hrname_r == NULL)
		rf->hrname_r = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				NULL);
	if (rf->hrdel_feed == NULL)
		rf->hrdel_feed = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				NULL);
	if (rf->hrdel_days == NULL)
		rf->hrdel_days = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				NULL);
	if (rf->hrdel_messages == NULL)
		rf->hrdel_messages = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				NULL);
	if (rf->hrdel_unread == NULL)
		rf->hrdel_unread =
			g_hash_table_new_full(
				g_str_hash,
				g_str_equal,
				g_free,
				NULL);
	if (rf->hrdel_notpresent == NULL)
		rf->hrdel_notpresent =
			g_hash_table_new_full(
				g_str_hash,
				g_str_equal,
				g_free,
				NULL);
	if (rf->hrttl == NULL)
		rf->hrttl = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				NULL);
	if (rf->hrttl_multiply == NULL)
		rf->hrttl_multiply = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				NULL);
	if (rf->hrupdate == NULL)
		rf->hrupdate = g_hash_table_new_full(g_str_hash,
				g_str_equal,
				g_free,
				NULL);

	if (!rf->activity)	//keeping track of taskbar operations
		rf->activity = g_hash_table_new_full(
					g_str_hash,
					g_str_equal,
					NULL, NULL);
	if (!rf->error_hash)	//keeping trask of taskbar errors
		rf->error_hash = g_hash_table_new_full(
					g_str_hash,
					g_str_equal,
					g_free, NULL);

	if (!rf->session)
		rf->session = g_hash_table_new(
				g_direct_hash, g_direct_equal);
	if (!rf->abort_session)
		rf->abort_session = g_hash_table_new(
					g_direct_hash, g_direct_equal);
	if (!rf->key_session)
		rf->key_session = g_hash_table_new(
					g_direct_hash, g_direct_equal);
}

void
finish_setup_feed(SoupSession *soup_sess,
		SoupMessage *msg,
		add_feed *user_data);

void
finish_setup_feed(
	SoupSession *soup_sess,
	SoupMessage *msg,
	add_feed *user_data)
{
	guint ret = 0;
	guint ttl;
	add_feed *feed = (add_feed *)user_data;
	RDF *r = NULL;
	GString *content = NULL;
	gchar *chn_name = NULL, *tmp_chn_name = NULL, *tmp = NULL;
	gchar *rssurl, *ver;
	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;
	gchar *tmsgkey;
	GError *err = NULL;
	gchar *tmsg = feed->tmsg;
	gpointer crc_feed = gen_md5(feed->feed_url);

	if (rf->cancel_all || rf->import_cancel)
		goto out;

	r = g_new0 (RDF, 1);
	feed_new = TRUE;
	r->shown = TRUE;

	prepare_hashes();

	rf->pending = TRUE;
	tmsgkey = tmsg;
	taskbar_op_set_progress(tmsgkey, tmsg, 0.4);

	if (msg->status_code != SOUP_STATUS_OK &&
		msg->status_code != SOUP_STATUS_CANCELLED) {
		g_set_error(&err, NET_ERROR, NET_ERROR_GENERIC, "%s",
			soup_status_get_phrase(msg->status_code));
		rss_error(crc_feed,
			feed->feed_name ? feed->feed_name: _("Unamed feed"),
			_("Error while setting up feed."),
			err->message);
		goto out;
	}

	if (!msg->response_body->length)
		goto out;

	if (msg->status_code == SOUP_STATUS_CANCELLED)
		goto out;

	content = g_string_new_len((gchar *)(msg->response_body->data),
			msg->response_body->length);

	xmlSubstituteEntitiesDefaultValue = 0;
	doc = xml_parse_sux (content->str, content->len);
	d("content:\n%s\n", content->str);
	root = xmlDocGetRootElement(doc);
	taskbar_op_set_progress(tmsgkey, tmsg, 0.5);

	if ((doc != NULL && root != NULL)
		&& (strcasestr((char *)root->name, "rss")
		|| strcasestr((char *)root->name, "rdf")
		|| strcasestr((char *)root->name, "feed"))) {
		r->cache = doc;
		r->uri = feed->feed_url;
		r->progress = feed->progress;

		//we preprocess feed first in order to get name, icon, etc
		//and later display the actual feed (once rf-> structure is
		//properly populated
		chn_name = process_feed(r);
//		taskbar_op_set_progress(tmsgkey, tmsg, 0.6);

add:
		//feed name can only come from an import so we rather prefer
		//resulted channel name instead of supplied one
		if (feed->feed_name && !chn_name) {
			chn_name = g_strdup(feed->feed_name);
		//	feed->orig_name = r->title;
		//	r->title = chn_name;
		}
		if (chn_name == NULL || !strlen(chn_name)) {
			chn_name = g_strdup (_(DEFAULT_NO_CHANNEL));
			r->title = chn_name;
		}

		tmp_chn_name = chn_name;
		chn_name = sanitize_folder(chn_name);
		tmp = chn_name;
		chn_name = generate_safe_chn_name(chn_name);

		g_hash_table_insert(rf->hrname,
			g_strdup(chn_name),
			g_strdup(crc_feed));
		g_hash_table_insert(rf->hrname_r,
			g_strdup(crc_feed),
			g_strdup(chn_name));
		g_hash_table_insert(rf->hr,
			g_strdup(crc_feed),
			g_strdup(feed->feed_url));
		g_hash_table_insert(rf->hre,
			g_strdup(crc_feed),
			GINT_TO_POINTER(feed->enabled));
		g_hash_table_insert(rf->hrdel_feed,
			g_strdup(crc_feed),
			GINT_TO_POINTER(feed->del_feed));
		g_hash_table_insert(rf->hrdel_days,
			g_strdup(crc_feed),
			GINT_TO_POINTER(feed->del_days));
		g_hash_table_insert(rf->hrdel_messages,
			g_strdup(crc_feed),
			GINT_TO_POINTER(feed->del_messages));
		g_hash_table_insert(
			rf->hrdel_unread,
			g_strdup(crc_feed),
			GINT_TO_POINTER(feed->del_unread));
		g_hash_table_insert(
			rf->hrdel_notpresent,
			g_strdup(crc_feed),
			GINT_TO_POINTER(feed->del_notpresent));
		r->ttl = r->ttl ? r->ttl : DEFAULT_TTL;
		if (feed->update == 2)
			ttl = feed->ttl;
		else
			ttl = r->ttl;
		g_hash_table_insert(rf->hrttl,
			g_strdup(crc_feed),
			GINT_TO_POINTER(ttl));
		g_hash_table_insert(rf->hrttl_multiply,
			g_strdup(crc_feed),
			GINT_TO_POINTER(feed->ttl_multiply));
		custom_feed_timeout();
		g_hash_table_insert(rf->hrupdate,
			g_strdup(crc_feed),
			GINT_TO_POINTER(feed->update));
		taskbar_op_set_progress(tmsgkey, tmsg, 0.8);

		ver = NULL;
		if (r->type && r->version)
			ver = g_strconcat(r->type, " ", r->version, NULL);
		else
			ver = g_strdup("-");

		g_hash_table_insert(rf->hrt,
			g_strdup(crc_feed),
			ver);

		g_hash_table_insert(rf->hrh,
			g_strdup(crc_feed),
			GINT_TO_POINTER(feed->fetch_html));

		if (feed->edit) {
			gchar *a = g_build_path(
					G_DIR_SEPARATOR_S,
					feed->prefix ? feed->prefix : "",
					feed->feed_name,
					NULL);
			gchar *b = g_build_path(
					G_DIR_SEPARATOR_S,
					r->title,
					NULL);
			update_feed_folder(b, a, 0);
			//r->title = feed->feed_name;
			r->title = a;
			g_free(b);
		}

		if (rf->import && feed->prefix) {
			gchar *a = g_build_path(
					G_DIR_SEPARATOR_S,
					feed->prefix ? feed->prefix : "",
					feed->feed_name,
					NULL);
			gchar *b = g_build_path(
					G_DIR_SEPARATOR_S,
					r->title,
					NULL);
			update_feed_folder(b, a, 0);
			g_free(a);
			g_free(b);
		}
		if (rf->treeview)
			store_redraw(GTK_TREE_VIEW(rf->treeview));
		save_gconf_feed();

//		if (feed->validate && !(rf->import && rf->display_cancel))
//			display_feed(r);
		g_idle_add(
			(GSourceFunc)display_feed_async,
			g_strdup(chn_name));

		if (rf->cancel_all || rf->import_cancel)
			goto out;


		taskbar_op_set_progress(tmsgkey, tmsg, 0.9);


		g_free(tmp_chn_name);
		g_free(tmp);
		g_free(chn_name);

/*		if (r->cache)
			xmlFreeDoc(r->cache);
		if (r->type)
			g_free(r->type);
		if (r->uids)
			g_array_free(r->uids, TRUE);
		if (r)
			g_free(r);
		if (content)
			g_string_free(content, 1);*/

		rf->setup = 1;
		ret = 1;
		goto out;
	}

	//search for a feed entry
	if (gconf_client_get_bool (rss_gconf, GCONF_KEY_SEARCH_RSS, NULL)) {
		dp("searching new feed\n");
		rssurl = search_rss(content->str, content->len);
		if (rssurl) {
			if (doc)
				xmlFreeDoc(doc);
//				if (r->type)
//					g_free(r->type);
			if (content)
				g_string_free(content, 1);
			feed->feed_url = rssurl;
			g_print("rssurl:%s|\n", rssurl);

			if (g_hash_table_find(
					rf->hr,
					check_if_match,
					feed->feed_url)) {
				rss_error(NULL, NULL,
					_("Error adding feed."),
					_("Feed already exists!"));
				goto out;
			}
			g_warning("Searching FOR feeds broken\n");
			//setup_feed(g_memdup(feed, sizeof(feed)));
			goto out;
		}
	}

	dp("general error\n");
	rss_error(crc_feed, NULL,
		_("Error while fetching feed."),
		_("Invalid Feed"));

out:	rf->pending = FALSE;
	if (rf->import) {
		rf->import--;
		d("IMPORT queue size:%d\n", rf->import);
		progress++;
		update_progress_bar(rf->import);
	}

	if (!rf->import) {
		if (rf->progress_dialog)
			gtk_widget_destroy(rf->progress_dialog);
		rf->progress_bar = NULL;
		rf->progress_dialog = NULL;
		progress = 0;
		rf->display_cancel = 0;
		rf->import_cancel = 0;
		rf->cancel_all = 0;
	}
	if (!rf->setup && feed->cancelable != NULL) {
		void (*f)() = (GFunc)feed->cancelable;
		f(feed->cancelable_arg);
	} else if (feed->ok != NULL) {
		void (*f)() = (GFunc)feed->ok;
		f(feed->ok_arg);
	}
	taskbar_op_finish(crc_feed);
	g_free(crc_feed);
	g_free(feed->feed_url);
	if (feed->feed_name) g_free(feed->feed_name);
	if (feed->prefix) g_free(feed->prefix);
	g_free(feed->tmsg);
	g_free(feed);
}

gboolean
setup_feed(add_feed *feed)
{
	GError *err = NULL;
	gchar *tmsg, *tmpkey;

	tmsg = g_strdup_printf(_("Adding feed %s"),
		feed->feed_name ? feed->feed_name :"unnamed");
	feed->tmsg = tmsg;
	taskbar_op_message(tmsg, gen_md5(feed->feed_url));

	check_folders();

	rf->setup = 0;
	rf->pending = TRUE;

//	if (!feed->validate)
//		goto add;

	d("adding feed->feed_url:%s\n", feed->feed_url);
	fetch_unblocking(
		feed->feed_url,
		textcb,
		g_strdup(feed->feed_url),
		(gpointer)finish_setup_feed,
		feed,	// we need to dupe key here
		1,
		&err);	// because we might lose it if
	if (err) {
		g_print("setup_feed() -> err:%s\n", err->message);
		tmpkey = gen_md5(feed->feed_url);
		rss_error(tmpkey,
			feed->feed_name ? feed->feed_name: _("Unamed feed"),
			_("Error while fetching feed."),
			err->message);
		g_free(tmpkey);
	}
	return TRUE;
}

void
update_sr_message(void)
{
	if (flabel && farticle) {
		gchar *fmsg = g_strdup_printf(
				_("Getting message %d of %d"),
				farticle,
				ftotal);
		gtk_label_set_text (GTK_LABEL (flabel), fmsg);
		g_free(fmsg);
	}
}

void
update_ttl(gpointer key, guint value)
{
	if (2 != GPOINTER_TO_INT(g_hash_table_lookup(rf->hrupdate, key)))
		g_hash_table_replace(
			rf->hrttl, g_strdup(key),
			GINT_TO_POINTER(value));
}


void
#if LIBSOUP_VERSION < 2003000
finish_feed (SoupMessage *msg, gpointer user_data)
#else
finish_feed (SoupSession *soup_sess, SoupMessage *msg, gpointer user_data)
#endif
{
	rfMessage *rfmsg = g_new0(rfMessage, 1);
	rfmsg->status_code = msg->status_code;
#if LIBSOUP_VERSION < 2003000
	rfmsg->body = msg->response.body;
	rfmsg->length = msg->response.length;
#else
	rfmsg->body = (gchar *)(msg->response_body->data);
	rfmsg->length = msg->response_body->length;
#endif
	generic_finish_feed(rfmsg, user_data);
	g_free(rfmsg);
}

#if EVOLUTION_VERSION < 22900 //kb//
struct _MailComponentPrivate {
	GMutex *lock;

	/* states/data used during shutdown */
#if EVOLUTION_VERSION >= 22801
	enum { MC_QUIT_NOT_START, MC_QUIT_START, MC_QUIT_SYNC, MC_QUIT_THREADS } quit_state;
#else
	enum { MC_QUIT_START, MC_QUIT_SYNC, MC_QUIT_THREADS } quit_state;
#endif
};
#endif

void
generic_finish_feed(rfMessage *msg, gpointer user_data)
{
	GError *err = NULL;
	gchar *chn_name = NULL;
	//FIXME user_data might be out of bounds here
	gchar *key =  lookup_key(user_data);
	gchar *tmsg;
	gboolean deleted = 0;
	GString *response;
	RDF *r;
#if (EVOLUTION_VERSION < 22900)
	MailComponent *mc = mail_component_peek ();
#endif

	//feed might get deleted while fetching
	//so we need to test for the presence of key
	if (!key)
		deleted = 1;

#if EVOLUTION_VERSION < 22900 //kb//
#if EVOLUTION_VERSION > 22801
	if (mc->priv->quit_state != MC_QUIT_NOT_START) {
#else
	if (mc->priv->quit_state != -1) {
#endif
		rf->cancel_all=1;
	}
#endif

	d("taskbar_op_finish() queue:%d\n", rf->feed_queue);

	if (rf->feed_queue) {
		rf->feed_queue--;
		tmsg = g_strdup_printf(_("Fetching Feeds (%d enabled)"),
			rss_find_enabled());
		taskbar_op_set_progress((gchar *)"main",
			tmsg,
			rf->feed_queue ? 1-(gdouble)((rf->feed_queue*100/rss_find_enabled()))/100 : 1);
		g_free(tmsg);
	}

	if (rf->feed_queue == 0) {
		d("taskbar_op_finish()\n");
		taskbar_op_finish(key);
		taskbar_op_finish(NULL);
		rf->autoupdate = FALSE;
		farticle=0;
		ftotal=0;
#ifndef EVOLUTION_2_12
		if(rf->progress_dialog) {
			gtk_widget_destroy(rf->progress_dialog);
			rf->progress_dialog = NULL;
			rf->progress_bar = NULL;
		}
#else
		if(rf->label && rf->info) {
			gtk_label_set_markup (GTK_LABEL (rf->label), _("Complete."));
			if (rf->info->cancel_button)
				gtk_widget_set_sensitive(rf->info->cancel_button, FALSE);
			gtk_progress_bar_set_fraction(
				(GtkProgressBar *)rf->progress_bar, 1);

			g_hash_table_steal(rf->info->data->active, rf->info->uri);
			rf->info->data->infos = g_list_remove(rf->info->data->infos, rf->info);

			if (g_hash_table_size(rf->info->data->active) == 0) {
				if (rf->info->data->gd)
					gtk_widget_destroy((GtkWidget *)rf->info->data->gd);
			}
			//clean data that might hang on rf struct
			rf->sr_feed = NULL;
			rf->label = NULL;
			flabel = NULL;
			rf->progress_bar = NULL;
			rf->info = NULL;
		}
#endif
	}

	if (rf->cancel_all)
		goto out;

	if (msg->status_code != SOUP_STATUS_OK &&
	    msg->status_code != SOUP_STATUS_CANCELLED) {
		g_set_error(&err,
			NET_ERROR,
			NET_ERROR_GENERIC,
			"%s",
			soup_status_get_phrase(msg->status_code));
		tmsg = g_strdup_printf("\n%s\n%s",
			(gchar *)user_data,
			(gchar *)err->message);
		rss_error(user_data,
			NULL,
			_("Error fetching feed."),
			tmsg);
		g_free(tmsg);
		goto out;
	}

	if (rf->cancel) {
#ifdef EVOLUTION_2_12
		if(rf->label && rf->feed_queue == 0 && rf->info) {
			farticle=0;
			ftotal=0;
			gtk_label_set_markup (GTK_LABEL (rf->label),
				_("Canceled."));
		if (rf->info->cancel_button)
			gtk_widget_set_sensitive(rf->info->cancel_button,
				FALSE);

		g_hash_table_steal(rf->info->data->active,
			rf->info->uri);
		rf->info->data->infos =
			g_list_remove(rf->info->data->infos,
					rf->info);

		if (g_hash_table_size(rf->info->data->active) == 0) {
			if (rf->info->data->gd)
				gtk_widget_destroy((GtkWidget *)rf->info->data->gd);
		}
		taskbar_op_finish(key);
		taskbar_op_finish(NULL);
		//clean data that might hang on rf struct
		rf->sr_feed = NULL;
		rf->label = NULL;
		flabel = NULL;
		rf->progress_bar = NULL;
		rf->info = NULL;
		}
#endif
		goto out;
	}

	if (!msg->length)
		goto out;

	if (msg->status_code == SOUP_STATUS_CANCELLED)
		goto out;


	response = g_string_new_len(msg->body, msg->length);

	g_print("feed %s\n", (gchar *)user_data);

	while (gtk_events_pending ())
		gtk_main_iteration ();

	r = g_new0 (RDF, 1);
	r->shown = TRUE;
	xmlSubstituteEntitiesDefaultValue = 1;
	r->cache = xml_parse_sux (response->str, response->len);
	if (rsserror) {
		xmlError *err = xmlGetLastError();
		gchar *tmsg = g_strdup_printf("\n%s\nInvalid feed: %s",
				(gchar *)user_data,
				err ? err->message : _("illegal content type!"));
		rss_error(user_data,
			NULL,
			_("Error while parsing feed."),
			tmsg);
		g_free(tmsg);
		goto cleanup;
	}

	if (msg->status_code == SOUP_STATUS_CANCELLED)
		goto cleanup;

	if (!deleted) {
		if (!user_data || !lookup_key(user_data))
			goto cleanup;
		r->uri =  g_hash_table_lookup(
				rf->hr, lookup_key(user_data));

		chn_name = display_doc (r);

		if (chn_name && strlen(chn_name)) {
			if (g_ascii_strcasecmp(user_data, chn_name) != 0) {
				gchar *md5 = g_strdup(
					g_hash_table_lookup(
						rf->hrname, user_data));
				g_hash_table_remove(rf->hrname_r, md5);
				g_hash_table_remove(rf->hrname, user_data);
				g_hash_table_insert(
					rf->hrname,
					g_strdup(chn_name), md5);
				g_hash_table_insert(
					rf->hrname_r, g_strdup(md5),
					g_strdup(chn_name));
				save_gconf_feed();
				update_ttl(md5, r->ttl);
				//save_data = user_data;
				user_data = chn_name;
			}
			if (g_hash_table_lookup(rf->hrdel_feed, lookup_key(user_data)))
				get_feed_age(r, user_data);
		}
	}
	//ftotal+=r->total;
	update_sr_message();
	g_string_free(response, 1);

//tout:

#ifdef EVOLUTION_2_12
	if (rf->sr_feed && !deleted) {
		gchar *furl = g_markup_printf_escaped(
				"<b>%s</b>: %s",
				_("Feed"),
				(gchar *)user_data);
		gtk_label_set_markup (GTK_LABEL (rf->sr_feed), furl);
		gtk_label_set_justify(GTK_LABEL (rf->sr_feed), GTK_JUSTIFY_LEFT);
		g_free(furl);
	}
	if(rf->label && rf->feed_queue == 0 && rf->info) {
		farticle=0;
		ftotal=0;
		gtk_label_set_markup (GTK_LABEL (rf->label), _("Complete"));
		if (rf->info->cancel_button)
			gtk_widget_set_sensitive(rf->info->cancel_button, FALSE);

		g_hash_table_steal(rf->info->data->active, rf->info->uri);
		rf->info->data->infos = g_list_remove(rf->info->data->infos, rf->info);

		if (g_hash_table_size(rf->info->data->active) == 0) {
			if (rf->info->data->gd)
				gtk_widget_destroy((GtkWidget *)rf->info->data->gd);
		}
		taskbar_op_finish(key);
		taskbar_op_finish(NULL);
		//clean data that might hang on rf struct
		rf->sr_feed = NULL;
		rf->label = NULL;
		flabel = NULL;
		rf->progress_bar = NULL;
		rf->info = NULL;
	}
#endif
cleanup:if (r->cache)
		xmlFreeDoc(r->cache);
	if (r->type)
		g_free(r->type);
	if (r->version)
		g_free(r->version);
	if (r->uids)
		g_array_free(r->uids, TRUE);
	g_free(r);
out:	if (chn_name) { //user_data
		//not sure why it dies here
		if (!rf->cancel && !rf->cancel_all)
			g_free(chn_name); //user_data
	}
	return;
}

gboolean
display_feed_async(gpointer key)
{
	GError *err = NULL;
	gchar *msg;
	gchar *url = g_hash_table_lookup(rf->hr, lookup_key(key));
	fetch_unblocking(
			url,
			NULL,
			key,
			(gpointer)finish_feed,
			g_strdup(key),	// we need to dupe key here
			1,
			&err);		// because we might lose it if
					// feed gets deleted
	if (err) {
		msg = g_strdup_printf("\n%s\n%s",
				(gchar *)key,
				err->message);
		rss_error(key,
			NULL,
			_("Error fetching feed."),
			msg);
			g_free(msg);
	}
	return FALSE;
}

gboolean
fetch_one_feed(gpointer key, gpointer value, gpointer user_data)
{
	GError *err = NULL;
	gchar *msg;
	gchar *url = g_hash_table_lookup(rf->hr, lookup_key(key));

	// check if we're enabled and no cancelation signal pending
	// and no imports pending
	// reject empty urls as we react kinda weird to them
	if (g_hash_table_lookup(rf->hre, lookup_key(key))
		&& strlen(url)
		&& !rf->cancel && !rf->import) {
		d("\nFetching: %s..%s\n", url, (gchar *)key);
		rf->feed_queue++;

		fetch_unblocking(
				url,
				user_data,
				key,
				(gpointer)finish_feed,
				g_strdup(key),	// we need to dupe key here
				1,
				&err);		// because we might lose it if
						// feed gets deleted
		if (err) {
			rf->feed_queue--;
			msg = g_strdup_printf("\n%s\n%s",
					(gchar *)key,
					err->message);
			rss_error(key,
				NULL,
				_("Error fetching feed."),
				msg);
			g_free(msg);
		}
		return TRUE;
	} else if (rf->cancel && !rf->feed_queue) {
		rf->cancel = 0;		//all feeds were either procesed or skipped
	}
	return FALSE;
}

gboolean
fetch_feed(gpointer key, gpointer value, gpointer user_data)
{
	//exclude feeds that have special update interval or
	//no update at all
	if (GPOINTER_TO_INT(g_hash_table_lookup(rf->hrupdate, lookup_key(key))) >= 2
		&& !force_update)
		return 0;

	return fetch_one_feed(key, value, user_data);
}

void
#if LIBSOUP_VERSION < 2003000
finish_website (SoupMessage *msg, gpointer user_data)
#else
finish_website (SoupSession *soup_sess, SoupMessage *msg, gpointer user_data)
#endif
{
	GString *response;
	gchar *tmsg, *str;
	gint len;
	UB* ub = (UB*)user_data;

	g_return_if_fail(rf->mozembed);

	response = g_string_new_len(msg->response_body->data,
			msg->response_body->length);
	d("browser full:%d\n", (int)response->len);
	d("browser fill:%d\n", (int)browser_fill);
	if (!response->len) {
		tmsg = g_strdup(_("Formatting error."));
		browser_write(
			tmsg, strlen(tmsg),
			(gchar *)"file:///fakefile#index");
		g_free(tmsg);
		if (ub->create) {
			//stream remove
			camel_stream_close(ub->stream);
#if (DATASERVER_VERSION >= 2031001)
			g_object_unref(ub->stream);
#else
			camel_object_unref(ub->stream);
#endif
		}
	} else {
		if (ub->create) {
			camel_stream_write(ub->stream, response->str, strlen(response->str));
			camel_stream_close(ub->stream);
#if (DATASERVER_VERSION >= 2031001)
			g_object_unref(ub->stream);
#else
			camel_object_unref(ub->stream);
#endif
		}
		str = (response->str);
		len = strlen(response->str);
		*str+= browser_fill;
		len-= browser_fill;
		browser_write(str, len, ub->url);
		g_string_free(response, 1);
	}
	browser_fill = 0;
}

void
#if LIBSOUP_VERSION < 2003000
finish_comments (SoupMessage *msg, EMFormatHTML *user_data);
#else
finish_comments (SoupSession *soup_sess, SoupMessage *msg, EMFormatHTML *user_data);
#endif

void
#if LIBSOUP_VERSION < 2003000
finish_comments (SoupMessage *msg, EMFormatHTML *user_data)
#else
finish_comments (SoupSession *soup_sess, SoupMessage *msg, EMFormatHTML *user_data)
#endif
{
	guint reload=0;
	GString *response;

	comments_session = g_slist_remove(comments_session, soup_sess);

//	if (!msg->length)
	//	goto out;

//	if (msg->status_code == SOUP_STATUS_CANCELLED)
//		goto out;

	response = g_string_new_len(msg->response_body->data, msg->response_body->length);

	if (!commstream)
		reload = 1;

	commstream = response->str;
	g_string_free(response, 0);

	if (reload && !rf->cur_format) {
		em_format_redraw((EMFormat *)user_data);
	}

	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
refresh_cb (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	em_format_redraw((EMFormat *)pobject);
}

gchar *
print_comments(gchar *url, gchar *stream, EMFormatHTML *format)
{
	RDF *r = NULL;
	xmlDocPtr doc;
	xmlNodePtr root;
	r = g_new0 (RDF, 1);
	r->shown = TRUE;
	doc = NULL;
	root = NULL;
	xmlSubstituteEntitiesDefaultValue = 0;
	doc = xml_parse_sux (stream, strlen(stream));
	d("content:\n%s\n", stream);
	root = xmlDocGetRootElement(doc);

	if ((doc != NULL && root != NULL)
		&& (strcasestr((char *)root->name, "rss")
		|| strcasestr((char *)root->name, "rdf")
		|| strcasestr((char *)root->name, "feed"))) {
			r->cache = doc;
			r->uri = url;

			return display_comments (r, format);
	}
	return NULL;
}


void
fetch_comments(gchar *url, EMFormatHTML *stream)
{
	GError *err = NULL;
	SoupSession *comm_sess = NULL;
	gchar *uniqcomm;

	d("\nFetching comments from: %s\n", url);
	uniqcomm = g_strdup_printf("COMMENT-%s", url);

	fetch_unblocking(
			url,
			NULL,
			uniqcomm,
			(gpointer)finish_comments,
			stream,	// we need to dupe key here
			1,
			&err);
	comm_sess = g_hash_table_lookup(rf->key_session, uniqcomm);
	comments_session = g_slist_append(comments_session, comm_sess);

	if (err) {
		gchar *msg = g_strdup_printf("\n%s\n%s",
				url, err->message);
		rss_error(url, NULL, _("Error fetching feed."), msg);
		g_free(msg);
	}
}

gboolean
update_articles(gboolean disabler)
{
#if (DATASERVER_VERSION >= 2031002)
	gboolean online =  camel_session_get_online (session);
#else
	gboolean online =  camel_session_is_online (session);
#endif
#if EVOLUTION_VERSION < 22900 //kb//
	MailComponent *mc = mail_component_peek ();
#if EVOLUTION_VERSION > 22801
	if (mc->priv->quit_state != MC_QUIT_NOT_START)
#else
	if (mc->priv->quit_state != -1)
#endif
		rf->cancel=1;
#endif

	if (!rf->pending && !rf->feed_queue && !rf->cancel_all && online) {
		g_print("Reading RSS articles...\n");
		rf->autoupdate = TRUE;
		rf->pending = TRUE;
		check_folders();
		rf->err = NULL;
		taskbar_op_message(NULL, NULL);
		network_timeout();
		g_hash_table_foreach(rf->hrname,
			(GHFunc)fetch_feed, (GHFunc *)statuscb);
		rf->pending = FALSE;
	}
	return disabler;
}

gchar *
rss_component_peek_base_directory(void)
{
#if (EVOLUTION_VERSION >= 22900) //kb//
	return g_strdup_printf("%s" G_DIR_SEPARATOR_S "rss",
//		em_utils_get_data_dir());
		mail_session_get_data_dir ());
#else
	MailComponent *component = mail_component_peek();
/* http://bugzilla.gnome.org/show_bug.cgi?id=513951 */
#if (EVOLUTION_VERSION >= 22300)		// include devel too
	return g_strdup_printf("%s" G_DIR_SEPARATOR_S "rss",
		mail_component_peek_base_directory (component));
#else
	return g_strdup_printf("%s" G_DIR_SEPARATOR_S "mail" G_DIR_SEPARATOR_S "rss",
		mail_component_peek_base_directory (component));
#endif
#endif
}

CamelStore *
rss_component_peek_local_store(void)
{
#if (EVOLUTION_VERSION < 22900) //kb//
	return mail_component_peek_local_store(NULL);
#else
	return e_mail_local_get_store();
#endif
}


//
//lookups main feed folder name
//this can be different from the default if folder was renamed
//
//
gchar *
lookup_main_folder(void)
{
	return rf->main_folder;
}

//lookup original name based on renamed folder
//
gchar *
lookup_original_folder(gchar *folder, gboolean *found)
{
	gchar *tmp = NULL, *ofolder = NULL;

	tmp = extract_main_folder(folder);
	if (tmp) {
		ofolder = g_hash_table_lookup(rf->feed_folders, tmp);
		d("result ofolder:%s\n", ofolder);
		if (ofolder) {
			g_free(tmp);
			if (found) *found = TRUE;
			return g_strdup(ofolder);
		} else {
			if (found) *found = FALSE;
			return tmp;
		}
	}
	return NULL;
}

//
//lookups feed folder name
//this can be different from the default if folder was renamed
//

gchar *
lookup_feed_folder(gchar *folder)
{
	gchar *new_folder = g_hash_table_lookup(
				rf->reversed_feed_folders, folder);
	return new_folder ? new_folder : folder;
}

gchar *
lookup_chn_name_by_url(gchar *url)
{
	gpointer crc_feed = gen_md5(url);
	gchar *chn_name = g_hash_table_lookup(rf->hrname_r,
		g_strdup(crc_feed));
	g_free(crc_feed);
	return chn_name;
}

void
update_main_folder(gchar *new_name)
{
	FILE *f;
	gchar *feed_dir, *feed_file;

	if (rf->main_folder)
		g_free(rf->main_folder);
	rf->main_folder = g_strdup(new_name);

	feed_dir = rss_component_peek_base_directory();
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);
	feed_file = g_strdup_printf("%s/main_folder", feed_dir);
	g_free(feed_dir);
	if ((f = fopen(feed_file, "w"))) {
		fprintf(f, "%s", rf->main_folder);
		fclose(f);
	}
	g_free(feed_file);
}

void
write_feeds_folder_line(gpointer key, gpointer value, FILE *file)
{
	fprintf(file, "%s\n", (char *)key);
	fprintf(file, "%s\n", (char *)value);
}

void
populate_reversed(gpointer key, gpointer value, GHashTable *hash)
{
	g_hash_table_insert(hash, g_strdup(value), g_strdup(key));
}


GList *rebase_keys = NULL;

typedef struct _rebase_name rebase_name;
struct _rebase_name {
	gchar *oname;
	gchar *nname;
};

void rebase_feed(gchar *key, rebase_name *rn);

void
rebase_feed(gchar *key, rebase_name *rn)
{
	gchar *value = g_strdup(g_hash_table_lookup(rf->feed_folders, key));
	gchar *base_key = strextr(key, rn->oname);
	gchar *tmp = g_strconcat(rn->nname, base_key, NULL);
	g_hash_table_remove(rf->feed_folders, key);
	g_hash_table_insert(rf->feed_folders, tmp, value);
}

void
search_rebase(gpointer key, gpointer value, gchar *oname)
{
	gchar *tmp = g_strdup_printf("%s/", oname);
	if (!strncmp(key, tmp, strlen(tmp))) {
		rebase_keys = g_list_append(rebase_keys, key);
	}
}

void
rebase_feeds(gchar *old_name, gchar *new_name)
{
	gchar *oname = extract_main_folder(old_name);
	gchar *nname = extract_main_folder(new_name);
	rebase_name *rn = g_new0(rebase_name, 1);
	rn->oname = oname;
	rn->nname = nname;
	g_hash_table_foreach(rf->feed_folders,
			(GHFunc)search_rebase, oname);
	g_list_foreach(rebase_keys, (GFunc)rebase_feed, rn);
	g_list_free(rebase_keys);
	rebase_keys = NULL;
	sync_folders();
}

/*
 * sync feeds folders data on disk
 */

void
sync_folders(void)
{
	FILE *f;
	gchar *feed_dir, *feed_file;

	feed_dir = rss_component_peek_base_directory();
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);
	feed_file = g_strdup_printf("%s/feed_folders",
		feed_dir);
	g_free(feed_dir);
	f = fopen(feed_file, "wb");
	if (!f)
		return;

	if (!g_hash_table_size(rf->feed_folders))
		goto exit;

	g_hash_table_foreach(rf->feed_folders,
		(GHFunc)write_feeds_folder_line,
		(gpointer *)f);
	g_free(feed_file);
	g_hash_table_destroy(rf->reversed_feed_folders);
	rf->reversed_feed_folders = g_hash_table_new_full(g_str_hash,
			g_str_equal,
			g_free,
			g_free);
	g_hash_table_foreach(rf->feed_folders,
			(GHFunc)populate_reversed,
			rf->reversed_feed_folders);
exit:	fclose(f);
	return;
}

/*construct feed_folders file with rename allocation
 * old_name initial channel name
 * new_name renamed name
 */

gint
update_feed_folder(gchar *old_name, gchar *new_name, gboolean valid_folder)
{
	gchar *oname = extract_main_folder(old_name);
	gchar *nname = extract_main_folder(new_name);
	gchar *orig_name, *ofolder;

	if (!oname)
		oname = g_strdup(old_name);
	if (!nname)
		nname = g_strdup(new_name);
	orig_name = g_hash_table_lookup(rf->feed_folders, oname);
	if (!orig_name) {
		if (valid_folder) {
			ofolder = lookup_original_folder(old_name, NULL);	//perhaps supply found variable
			if (!ofolder)						// to test result
				return 0;
			else if (!lookup_key(ofolder))
				return 0;
		}
		g_hash_table_replace(
			rf->feed_folders,
			g_strdup(nname),
			g_strdup(oname));
	} else {
		g_hash_table_replace(
			rf->feed_folders,
			g_strdup(nname),
			g_strdup(orig_name));
		g_hash_table_remove(rf->feed_folders, oname);
	}

	sync_folders();
	g_free(oname);
	g_free(nname);
	return 1;
}

CamelFolder *
check_feed_folder(gchar *folder_name)
{
	CamelStore *store = rss_component_peek_local_store();
	CamelFolder *mail_folder;
	char **path = NULL;
	gint i=0;
	gchar *base_folder;

	gchar *main_folder = lookup_main_folder();
	gchar *real_folder = lookup_feed_folder(folder_name);
	gchar *real_name = g_strdup_printf(
				"%s" G_DIR_SEPARATOR_S "%s", main_folder, real_folder);
	d("main_folder:%s\n", main_folder);
	d("real_folder:%s\n", real_folder);
	d("real_name:%s\n", real_name);
	mail_folder = camel_store_get_folder (store, real_name, 0, NULL);
	base_folder = main_folder;
	if (mail_folder == NULL) {
		sanitize_path_separator(real_folder);
		path = g_strsplit(real_folder, G_DIR_SEPARATOR_S, 0);
		if (path) {
			do {
				if (path[i] == NULL)
					break;
				if (path[i] && strlen(path[i])) {
					camel_store_create_folder (
						store,
						base_folder,
						path[i],
						NULL);
					base_folder = g_strconcat(
							base_folder,
							"/",
							path[i],
							NULL);
				}
			} while (NULL != path[++i]);
			g_strfreev(path);
		}
		mail_folder = camel_store_get_folder (store,
				real_name,
				0,
				NULL);
	}
	g_free(real_name);
	return mail_folder;

}

void
rss_delete_feed(gchar *full_path, gboolean folder)
{
	CamelException ex;
	gchar *tmp, *tkey, *url;
	CamelStore *store;
	gchar *name, *real_name, *buf, *feed_dir, *feed_name;

	store = rss_component_peek_local_store();
	name = extract_main_folder(full_path);
	d("name to delete:'%s'\n", name);
	if (!name)
		return;
	real_name = g_hash_table_lookup(rf->feed_folders, name);
	if (!real_name)
		real_name = name;
	camel_exception_init (&ex);
	rss_delete_folders (store, full_path, &ex);
	if (camel_exception_is_set (&ex)) {
#if EVOLUTION_VERSION < 22904
		e_error_run(NULL,
			"mail:no-delete-folder",
			full_path,
			ex.desc,
			NULL);
#else
		e_alert_run_dialog_for_args(
			e_shell_get_active_window (NULL),
			"mail:no-delete-folder",
			full_path,
			ex.desc,
			NULL);
#endif
		camel_exception_clear (&ex);
	}
	//also remove status file
	tkey = g_hash_table_lookup(rf->hrname,
		real_name);
	if (!tkey)
		return;
	url =  g_hash_table_lookup(rf->hr, tkey);
	if (!url)
		goto out;
	buf = gen_md5(url);
	feed_dir = rss_component_peek_base_directory();
	feed_name = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", feed_dir, buf);
	g_free(feed_dir);
	g_free(buf);
	unlink(feed_name);
	tmp = g_strdup_printf("%s.img", feed_name);
	unlink(tmp);
	g_free(tmp);
	tmp = g_strdup_printf("%s.fav", feed_name);
	unlink(tmp);
	g_free(tmp);
out:	if (folder) {
		d("print folder:%s\n", real_name);
		remove_feed_hash(real_name);
	}
	delete_feed_folder_alloc(name);
	g_free(name);
	g_idle_add((GSourceFunc)store_redraw,
		GTK_TREE_VIEW(rf->treeview));
	save_gconf_feed();
}

static void
store_folder_deleted(CamelObject *o, void *event_data, void *data)
{
	CamelFolderInfo *info = event_data;
	d("Folder deleted '%s' full '%s'\n", info->name, info->full_name);
	rss_delete_feed(info->full_name, 1);
}

typedef struct {
	gchar *old_base;
	CamelFolderInfo *new;
} RenameInfo;

static void
store_folder_renamed(CamelObject *o, void *event_data, void *data)
{
	RenameInfo *info = event_data;

	gchar *main_folder = lookup_main_folder();
	if (!g_ascii_strncasecmp(info->old_base, main_folder, strlen(main_folder))
		|| !g_ascii_strncasecmp(info->old_base, OLD_FEEDS_FOLDER, strlen(OLD_FEEDS_FOLDER))) {
		d("Folder renamed to '%s' from '%s'\n",
			info->new->full_name, info->old_base);
		if (!g_ascii_strncasecmp(main_folder, info->old_base, strlen(info->old_base))
		|| !g_ascii_strncasecmp(OLD_FEEDS_FOLDER, info->old_base, strlen(info->old_base)))
			update_main_folder(info->new->full_name);
		else
			if (0 == update_feed_folder(info->old_base, info->new->full_name, 1)) {
				d("info->old_base:%s\n", info->old_base);
				d("info->new->full_name:%s\n",
					info->new->full_name);
				d("this is not a feed!!\n");
				rebase_feeds(
					info->old_base,
					info->new->full_name);
			}
		g_idle_add(
			(GSourceFunc)store_redraw,
			GTK_TREE_VIEW(rf->treeview));
		save_gconf_feed();
	}
}

typedef struct custom_fetch_data {
	gboolean disabler;
	gpointer key;
	gpointer value;
	gpointer user_data;
} CDATA;

gboolean custom_update_articles(CDATA *cdata);

gboolean
custom_update_articles(CDATA *cdata)
{
	GError *err = NULL;
	gchar *msg;
#if (DATASERVER_VERSION >= 2031002)
	gboolean online =  camel_session_get_online (session);
#else
	gboolean online =  camel_session_is_online (session);
#endif
	//if (!rf->pending && !rf->feed_queue && online)
	if (online) {
		g_print("Fetch (custom) RSS articles...\n");
		check_folders();
		rf->err = NULL;
		rf->autoupdate = TRUE;
		//taskbar_op_message();
		network_timeout();
		// check if we're enabled and no cancelation signal pending
		// and no imports pending
		dp("cdata->key:%s\n", (gchar *)cdata->key);
		if (g_hash_table_lookup(rf->hre, lookup_key(cdata->key))
		&& !rf->cancel && !rf->import) {
			d("\nFetching: %s..%s\n",
				(char *)g_hash_table_lookup(rf->hr,
					lookup_key(cdata->key)),
					(char *)cdata->key);
			rf->feed_queue++;

		fetch_unblocking(
			g_hash_table_lookup(rf->hr, lookup_key(cdata->key)),
			cdata->user_data,
			cdata->key,
			(gpointer)finish_feed,
			g_strdup(cdata->key), // we need to dupe key here
			1,
			&err);                // because we might lose it if
			if (err) {
				rf->feed_queue--;
				msg = g_strdup_printf("\n%s\n%s",
					(char *)cdata->key, err->message);
				rss_error(
					cdata->key,
					NULL,
					_("Error fetching feed."),
					msg);
				g_free(msg);
			}
		} else if (rf->cancel && !rf->feed_queue) {
			rf->cancel = 0;//all feeds where either procesed or skipped
		}
	}
	return TRUE;
}

gboolean
custom_fetch_feed(gpointer key, gpointer value, gpointer user_data)
{
	guint time_id = 0;
	guint ttl, ttl_multiply;
	if (!custom_timeout)
		custom_timeout = g_hash_table_new_full(
					g_str_hash, g_str_equal,
					g_free, NULL);

	if (GPOINTER_TO_INT(g_hash_table_lookup(rf->hrupdate, lookup_key(key))) == 2
	 && g_hash_table_lookup(rf->hre, lookup_key(key))) {
		d("custom key:%s\n", (char *)key);
		ttl = GPOINTER_TO_INT(
			g_hash_table_lookup(rf->hrttl, lookup_key(key)));
		ttl_multiply = GPOINTER_TO_INT(
				g_hash_table_lookup(
					rf->hrttl_multiply,
					lookup_key(key)));
		if (ttl) {
			CDATA *cdata = g_new0(CDATA, 1);
			cdata->key = key;
			cdata->value = value;
			cdata->user_data = user_data;
			time_id = GPOINTER_TO_INT(
					g_hash_table_lookup(
						custom_timeout,
						lookup_key(key)));
			if (time_id)
				g_source_remove(time_id);
			switch (ttl_multiply) {
				case 1:
					ttl_multiply = 60;
					break;
				case 2:
					ttl_multiply = 1440;
					break;
				default:
					ttl_multiply = 1;
					break;
			}
			time_id = g_timeout_add (
					ttl * 60 * 1000 * ttl_multiply,
					(GtkFunction) custom_update_articles,
					cdata);
			g_hash_table_replace(custom_timeout,
				g_strdup(lookup_key(key)),
				GINT_TO_POINTER(time_id));
			return 1;
		}
	}
	return 0;
}

void evo_window_popup(GtkWidget *win)
{
	gint x, y, sx, sy, new_x, new_y;
#if GTK_VERSION >= 2014000
	GdkWindow *window = gtk_widget_get_window(win);
#else
	GdkWindow *window = win->window;
#endif

	g_return_if_fail(win != NULL);
	g_return_if_fail(window != NULL);

	sx = gdk_screen_width();
	sy = gdk_screen_height();

	gdk_window_get_origin(window, &x, &y);
	new_x = x % sx; if (new_x < 0) new_x = 0;
	new_y = y % sy; if (new_y < 0) new_y = 0;
	if (new_x != x || new_y != y)
		gdk_window_move(window, new_x, new_y);

	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(win), FALSE);
	gtk_window_present(GTK_WINDOW(win));
#ifdef G_OS_WIN32
	/* ensure that the window is displayed at the top */
	gdk_window_show(window);
#endif
}

static void
icon_activated (GtkStatusIcon *icon, gpointer pnotify)
{
#if EVOLUTION_VERSION < 22900 //KB//
	GList *p, *pnext;
	for (p = (gpointer)evo_window; p != NULL; p = pnext) {
		pnext = p->next;

		if (gtk_window_is_active(GTK_WINDOW(p->data))) {
			gtk_window_iconify(GTK_WINDOW(p->data));
			gtk_window_set_skip_taskbar_hint(
				GTK_WINDOW(p->data),
				TRUE);
		} else {
			gtk_window_iconify(GTK_WINDOW(p->data));
			evo_window_popup(GTK_WIDGET(p->data));
			gtk_window_set_skip_taskbar_hint(
				GTK_WINDOW(p->data),
				FALSE);
		}
	}
#else
	if (gtk_window_is_active(GTK_WINDOW(evo_window))) {
		gtk_window_iconify(GTK_WINDOW(evo_window));
		gtk_window_set_skip_taskbar_hint(
			GTK_WINDOW(evo_window),
			TRUE);
	} else {
		gtk_window_iconify(GTK_WINDOW(evo_window));
		evo_window_popup(GTK_WIDGET(evo_window));
		gtk_window_set_skip_taskbar_hint(
			GTK_WINDOW(evo_window),
			FALSE);
		}
#endif
}

static void
create_status_icon(void)
{
	if (!status_icon) {
		gchar *iconfile = g_build_filename (EVOLUTION_ICONDIR,
			"rss-icon-unread.png",
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
	}
//     gtk_status_icon_set_visible (status_icon, FALSE);
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
update_status_icon(const char *channel, gchar *title)
{
	gchar *total;
	gchar *stext;
	if (gconf_client_get_bool (rss_gconf, GCONF_KEY_STATUS_ICON, NULL)) {
		total = g_strdup_printf("%s: %s\n\n", channel, title);
		create_status_icon();
		g_queue_push_tail(status_msg, total);
		//g_free(total);
		if (g_queue_get_length(status_msg) == 6)
			g_queue_pop_head(status_msg);
		g_queue_foreach(status_msg, flaten_status, flat_status_msg);
		stext = g_markup_escape_text(
				flat_status_msg,
				strlen(flat_status_msg));
#if GTK_VERSION >= 2016000
		gtk_status_icon_set_tooltip_text (status_icon, stext);
#else
		gtk_status_icon_set_tooltip (status_icon, stext);
#endif
		g_free(stext);
		gtk_status_icon_set_visible (status_icon, TRUE);
#if GTK_MINOR_VERSION < 22
		if (gconf_client_get_bool (rss_gconf, GCONF_KEY_BLINK_ICON, NULL)
		&& !gtk_status_icon_get_blinking(status_icon))
			gtk_status_icon_set_blinking (status_icon, TRUE);
#endif
		g_timeout_add(15 * 1000, flicker_stop, NULL);
		g_free(flat_status_msg);
		flat_status_msg = NULL;
	}
}

void
custom_feed_timeout(void)
{
	g_hash_table_foreach(
		rf->hrname,
		(GHFunc)custom_fetch_feed,
		statuscb);
}

#if EVOLUTION_VERSION < 22900 //KB//
struct __EShellPrivate {
	/* IID for registering the object on OAF.  */
	char *iid;

	GList *windows;

	//we do not care about the rest
};

typedef struct __EShellPrivate EShellPrivate;

struct _EShell {
	BonoboObject parent;

	EShellPrivate *priv;
};
typedef struct _EShell EShell;
#endif

void get_shell(void *ep, ESEventTargetShell *t)
{
#if EVOLUTION_VERSION < 22900 //KB//
	EShell *shell = t->shell;
	EShellPrivate *priv = (EShellPrivate *)shell->priv;
	evo_window = (GtkWidget *)priv->windows;
#endif
}

#if EVOLUTION_VERSION < 22900 //KB
void org_gnome_cooly_rss_startup(void *ep, EMPopupTargetSelect *t);

void org_gnome_cooly_rss_startup(void *ep, EMPopupTargetSelect *t)
#else
void org_gnome_cooly_rss_startup(void *ep, ESEventTargetUpgrade *t);

void org_gnome_cooly_rss_startup(void *ep, ESEventTargetUpgrade *t)
#endif
{
	gdouble timeout;
	CamelStore *store;

	if (gconf_client_get_bool (rss_gconf, GCONF_KEY_START_CHECK, NULL)) {
		//as I don't know how to set this I'll setup a 10 secs timeout
		//and return false for disableation
		g_timeout_add (3 * 1000,
			(GtkFunction) update_articles,
			0);
	}
	timeout = gconf_client_get_float(
			rss_gconf,
			GCONF_KEY_REP_CHECK_TIMEOUT,
			NULL);
	if (gconf_client_get_bool (rss_gconf, GCONF_KEY_REP_CHECK, NULL)) {
		rf->rc_id = g_timeout_add (60 * 1000 * timeout,
				(GtkFunction) update_articles,
				(gpointer)1);
	}
	custom_feed_timeout();

	rss_load_images();


	/* hook in rename event to catch feeds folder rename */
	store = rss_component_peek_local_store();
#if (DATASERVER_VERSION >= 2031002)
	g_signal_connect(store, "folder_renamed",
		G_CALLBACK(store_folder_renamed), NULL);
	g_signal_connect(store, "folder_deleted",
		G_CALLBACK(store_folder_deleted), NULL);
#else
	camel_object_hook_event(store, "folder_renamed",
		(CamelObjectEventHookFunc)store_folder_renamed, NULL);
	camel_object_hook_event(store, "folder_deleted",
		(CamelObjectEventHookFunc)store_folder_deleted, NULL);
#endif
}

/* check if rss folders exists and create'em otherwise */
void
check_folders(void)
{
	CamelException ex;
	CamelStore *store = rss_component_peek_local_store();
	CamelFolder *mail_folder, *old_folder;

	mail_folder = camel_store_get_folder (
			store, lookup_main_folder(), 0, NULL);
	old_folder = camel_store_get_folder (
			store, OLD_FEEDS_FOLDER, 0, NULL);
	if (old_folder) {
		camel_store_rename_folder(
			store, OLD_FEEDS_FOLDER,
			lookup_main_folder(), NULL);
	} else if (mail_folder == NULL) {
		camel_store_create_folder (
			store, NULL,
			lookup_main_folder(), &ex);
		return;
	}
#if (DATASERVER_VERSION >= 2031001)
	g_object_unref (mail_folder);
#else
	camel_object_unref (mail_folder);
#endif
}



gboolean
check_if_enabled (gpointer key, gpointer value, gpointer user_data)
{
	return GPOINTER_TO_INT(value);
}

#if 0
void org_gnome_cooly_rss_refresh(void *ep, EMPopupTargetSelect *t);

void
org_gnome_cooly_rss_refresh(void *ep, EMPopupTargetSelect *t)
{
#ifndef EVOLUTION_2_12
	GtkWidget *readrss_dialog;
	GtkWidget *readrss_label;
	GtkWidget *readrss_progress;
	GtkWidget *label,*progress_bar, *cancel_button, *status_label;

	rf->t = t;

	//don't waste anytime - we do not have network
	//should we fake it ? :D
	if (!rf->online)
		return;

	//no feeds enabled
	if (!g_hash_table_find(rf->hre, check_if_enabled, NULL))
		return;

	if (!rf->setup || g_hash_table_size(rf->hrname)<1) {
		taskbar_push_message(_("No RSS feeds configured!"));
		return;
	}

	readrss_dialog = e_error_new(NULL,
		"org-gnome-evolution-rss:readrss",
		_("Reading RSS feeds..."),
		NULL);

	g_signal_connect(readrss_dialog,
		"response",
		G_CALLBACK(readrss_dialog_cb),
		NULL);
	GtkWidget *label2 = gtk_label_new(NULL);
#if GTK_VERSION >= 2006000
	gtk_label_set_ellipsize (GTK_LABEL (label2), PANGO_ELLIPSIZE_START);
#endif
#if GTK_VERSION > 2008011
	gtk_label_set_justify(GTK_LABEL(label2), GTK_JUSTIFY_CENTER);
#endif
	readrss_label = gtk_label_new(_("Please wait"));
	if (!rf->progress_dialog) {
		readrss_progress = gtk_progress_bar_new();
		gtk_box_pack_start(GTK_BOX(((GtkDialog *)readrss_dialog)->vbox), label2, TRUE, TRUE, 10);
		gtk_box_pack_start(GTK_BOX(((GtkDialog *)readrss_dialog)->vbox), readrss_label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(((GtkDialog *)readrss_dialog)->vbox), readrss_progress, FALSE, FALSE, 0);
		gtk_progress_bar_set_fraction((GtkProgressBar *)readrss_progress, 0);
		/* xgettext:no-c-format */
		gtk_progress_bar_set_text((GtkProgressBar *)readrss_progress, _("0% done"));
		gtk_widget_show_all(readrss_dialog);
		rf->progress_dialog = readrss_dialog;
		rf->progress_bar = readrss_progress;
		rf->label       = label2;
		flabel       = label2;
	}
	if (!rf->pending && !rf->feed_queue) {
		rf->pending = TRUE;
		check_folders();

		rf->err = NULL;
		force_update = 1;
		taskbar_op_message(NULL);
		network_timeout();
		g_hash_table_foreach(rf->hrname, fetch_feed, statuscb);
		// reset cancelation signal
		if (rf->cancel)
			rf->cancel = 0;
		force_update = 0;
		rf->pending = FALSE;
	}
#endif
}
#endif

static void
set_send_status(struct _send_info *info, const char *desc, int pc)
{
	/* FIXME: LOCK */
	g_free(info->what);
	info->what = g_strdup(desc);
	info->pc = pc;
}

/* for camel operation status */
static void
my_op_status(CamelOperation *op, const char *what, int pc, void *data)
{
	struct _send_info *info = data;

	g_print("OP STATUS\n");
	g_print("CANCEL!!!!\n");

	switch (pc) {
	case CAMEL_OPERATION_START:
		pc = 0;
		break;
	case CAMEL_OPERATION_END:
		pc = 100;
		break;
	}

	set_send_status(info, what, pc);
}

static void
dialog_response(GtkDialog *gd, int button, struct _send_data *data)
{
	g_print("ABORTING...\n");
	abort_all_soup();
}

void
#ifdef EVOLUTION_2_12
org_gnome_evolution_rss(void *ep, EMEventTargetSendReceive *t);
#else
org_gnome_evolution_rss(void *ep, EMPopupTargetSelect *t);
#endif

void
#ifdef EVOLUTION_2_12
org_gnome_evolution_rss(void *ep, EMEventTargetSendReceive *t)
#else
org_gnome_evolution_rss(void *ep, EMPopupTargetSelect *t)
#endif
{
#ifdef EVOLUTION_2_12
	struct _send_info *info;
	struct _send_data *data = (struct _send_data *)t->data;
#endif

	GtkWidget *label,*progress_bar, *cancel_button, *status_label;
	GtkWidget *recv_icon;
	gchar *pretty_url;
	guint row;

	rf->t = t;

	//no feeds enabled
	if (!g_hash_table_find(rf->hre, check_if_enabled, NULL))
		return;

	if (g_hash_table_size(rf->hrname)<1) {
		taskbar_push_message(_("No RSS feeds configured!"));
		return;
	}

#ifdef EVOLUTION_2_12
	g_signal_connect(
		data->gd,
		"response",
		G_CALLBACK(dialog_response),
		NULL);

	info = g_malloc0 (sizeof (*info));
//        info->type = type;
//
	info->uri = g_strdup("feed"); //g_stddup

	info->cancel = camel_operation_new (my_op_status, info);
	info->state = SEND_ACTIVE;
//        info->timeout_id = g_timeout_add (STATUS_TIMEOUT, operation_status_timeout, info);
//
	g_hash_table_insert (data->active, info->uri, info);
//        list = g_list_prepend (list, info);

	recv_icon = gtk_image_new_from_stock (
			"rss-main", GTK_ICON_SIZE_LARGE_TOOLBAR);

	row = t->row;
	row+=2;
	t->row = row;

	gtk_table_resize(GTK_TABLE(t->table), t->row, 4);

	pretty_url = g_strdup ("RSS");
	label = gtk_label_new (NULL);
#if GTK_VERSION >= 2006000
	gtk_label_set_ellipsize (
		GTK_LABEL (label), PANGO_ELLIPSIZE_END);
#endif
#if GTK_VERSION > 2008011
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
#endif
	gtk_label_set_markup (GTK_LABEL (label), pretty_url);
	g_free (pretty_url);

	progress_bar = gtk_progress_bar_new ();

	cancel_button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);

	status_label = gtk_label_new (_("Waiting..."));
//                status_label = e_clipped_label_new (
//                    "www",
//                  PANGO_WEIGHT_BOLD, 1.0);

	gtk_misc_set_alignment (GTK_MISC (label), 0, .5);
	gtk_misc_set_alignment (GTK_MISC (status_label), 0, .5);

	gtk_table_attach (
		GTK_TABLE (t->table), recv_icon,
		0, 1, row, row+2, 0, 0, 0, 0);
	gtk_table_attach (
		GTK_TABLE (t->table), label,
		1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_table_attach (
		GTK_TABLE (t->table), progress_bar,
		2, 3, row, row+2, 0, 0, 0, 0);
	gtk_table_attach (
		GTK_TABLE (t->table), cancel_button,
		3, 4, row, row+2, 0, 0, 0, 0);
	gtk_table_attach (
		GTK_TABLE (t->table), status_label,
		1, 2, row+1, row+2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	g_signal_connect (
			cancel_button, "clicked",
			G_CALLBACK (receive_cancel), info);

	info->progress_bar = progress_bar;
	info->status_label = status_label;
	info->cancel_button = cancel_button;
	info->data = (struct _send_data *)t->data;
	rf->info = info;

	rf->progress_bar = progress_bar;
	rf->sr_feed	= label;
	rf->label	= status_label;
	flabel		= status_label;
#else

#if (EVOLUTION_VERSION >= 22900) //kb//
	EShell *shell;
	GtkWindow *parent;
	GList *windows;

	shell = e_shell_get_default ();
	windows = e_shell_get_watched_windows (shell);
	parent = (windows != NULL) ? GTK_WINDOW (windows->data) : NULL;

	readrss_dialog = e_alert_dialog_new_for_args(parent,
#else
	readrss_dialog = e_error_new(NULL,
#endif
		"org-gnome-evolution-rss:readrss",
		_("Reading RSS feeds..."), NULL);

	g_signal_connect(
		readrss_dialog,
		"response",
		G_CALLBACK(readrss_dialog_cb),
		NULL);
	GtkWidget *label2 = gtk_label_new(NULL);
	readrss_label = gtk_label_new(_("Please wait"));
	if (!rf->progress_dialog) {
		readrss_progress = gtk_progress_bar_new();
		gtk_box_pack_start(
			GTK_BOX(((GtkDialog *)readrss_dialog)->vbox),
			label2, TRUE, TRUE, 10);
		gtk_box_pack_start(
			GTK_BOX(((GtkDialog *)readrss_dialog)->vbox),
			readrss_label, FALSE, FALSE, 0);
		gtk_box_pack_start(
			GTK_BOX(((GtkDialog *)readrss_dialog)->vbox),
			readrss_progress, FALSE, FALSE, 0);
		gtk_progress_bar_set_fraction(
			(GtkProgressBar *)readrss_progress, 0);
		/* xgettext:no-c-format */
		gtk_progress_bar_set_text(
			(GtkProgressBar *)readrss_progress, _("0% done"));
		gtk_widget_show_all(readrss_dialog);
		rf->progress_dialog = readrss_dialog;
		rf->progress_bar = readrss_progress;
		rf->label	= label2;
		flabel		= label2;
	}
#endif
	if (!rf->pending && !rf->feed_queue) {
		rf->pending = TRUE;
		check_folders();

		rf->err = NULL;
		force_update = 1;
		taskbar_op_message(NULL, NULL);
		network_timeout();
		g_hash_table_foreach(
			rf->hrname, (GHFunc)fetch_feed, statuscb);
		// reset cancelation signal
		if (rf->cancel)
			rf->cancel = 0;
		force_update = 0;
		rf->pending = FALSE;
	}
}

void
rss_finalize(void)
{
	g_print("RSS: cleaning all remaining sessions ..");
	abort_all_soup();
	g_print(".done\n");
	if (rf->mozembed)
		gtk_widget_destroy(rf->mozembed);

/*	guint render = GPOINTER_TO_INT(
		gconf_client_get_int(rss_gconf,
			GCONF_KEY_HTML_RENDER,
			NULL));*/
#ifdef HAVE_GECKO
	/*/really find a better way to deal with this//
	//I do not know how to shutdown gecko (gtk_moz_embed_pop_startup)
	//crash in nsCOMPtr_base::assign_with_AddRef
#ifdef HAVE_BUGGY_GECKO
	if (2 == render)
		system("killall -SIGTERM evolution");
#else*/
	gecko_shutdown();
#endif
}

guint
fallback_engine(void)
{
#ifdef HAVE_RENDERKIT
	guint engine = gconf_client_get_int(
			rss_gconf, GCONF_KEY_HTML_RENDER, NULL);
#if !defined(HAVE_GECKO) && !defined (HAVE_WEBKIT)
	engine = 0;
#endif
if (engine == 2) {
#if !defined(HAVE_GECKO)
	engine = 1;
#endif
}
if (engine == 1) {
#if !defined (HAVE_WEBKIT)
	engine = 2;
#endif
}
	return engine;
#endif
	return 0;
}

#if EVOLUTION_VERSION >= 22900
void quit_cb(void *ep, EShellView *shell_view)
{
	g_print("RSS: Preparing to quit...\n");
	rf->cancel_all=1;
}

gboolean e_plugin_ui_init (GtkUIManager *ui_manager,
	EShellView *shell_view);

gboolean
e_plugin_ui_init (GtkUIManager *ui_manager,
	EShellView *shell_view)
{
	EShellWindow *shell_window;

	rss_shell_view = shell_view;
	shell_window = e_shell_view_get_shell_window (rss_shell_view);
	g_signal_connect (
		e_shell_window_get_action (
			E_SHELL_WINDOW (shell_window),
			"mail-folder-refresh"),
			"activate",
		G_CALLBACK (org_gnome_cooly_folder_refresh),
		rss_shell_view);
	g_signal_connect (
		e_shell_window_get_action (
			E_SHELL_WINDOW (shell_window),
			"quit"),
		"activate",
		G_CALLBACK (quit_cb),
		rss_shell_view);
#if EVOLUTION_VERSION >= 22900
/*	rss_search_bar_hook();*/
	webkit_hook_actions();
#endif
	return TRUE;
}
#endif


#if (EVOLUTION_VERSION < 22900)
int e_plugin_lib_enable(EPluginLib *ep, int enable);
#else
int e_plugin_lib_enable(EPlugin *ep, int enable);
#endif

int
#if (EVOLUTION_VERSION < 22900)
e_plugin_lib_enable(EPluginLib *ep, int enable)
#else
e_plugin_lib_enable(EPlugin *ep, int enable)
#endif
{
	char *d;
	guint render;

	if (enable) {
		bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
		bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	//	textdomain (GETTEXT_PACKAGE);
		rss_gconf = gconf_client_get_default();
		upgrade = 1;
		d = getenv("RSS_VERBOSE_DEBUG");
		if (d)
			rss_verbose_debug = atoi(d);

		//initiate main rss structure
		if (!rf) {
			printf("RSS Plugin enabled (evolution %s, evolution-rss %s)\n",
				EVOLUTION_VERSION_STRING,
				VERSION);
			rf = malloc(sizeof(rssfeed));
			memset(rf, 0, sizeof(rssfeed));
			read_feeds(rf);
			rf->pending = FALSE;
			rf->progress_dialog = NULL;
			rf->errdialog = NULL;
			rf->cancel = FALSE;
			rf->rc_id = 0;
			rf->feed_queue = 0;
			rf->main_folder = get_main_folder();
			rf->soup_auth_retry = 1;
			status_msg = g_queue_new();
			get_feed_folders();
			rss_build_stock_images();
			rss_cache_init();
#if (DATASERVER_VERSION >= 2023001)
			proxy = proxy_init();
#endif
			rss_soup_init();
#if HAVE_DBUS
			d("init_dbus()\n");
			/*D-BUS init*/
			rf->bus = init_dbus ();
#endif
			prepare_hashes();
			//there is no shutdown for e-plugin yet.
			atexit(rss_finalize);
			render = GPOINTER_TO_INT(
				gconf_client_get_int(rss_gconf,
						GCONF_KEY_HTML_RENDER,
						NULL));

			if (!render) {	// set render just in case it was forced in configure
				render = RENDER_N;
				gconf_client_set_int(
					rss_gconf,
					GCONF_KEY_HTML_RENDER,
					render, NULL);
			}
#ifdef HAVE_GECKO
			if (2 == render)
				rss_mozilla_init();
#endif
#if EVOLUTION_VERSION >= 22900
			init_rss_prefs();
#endif
		}
		upgrade = 2; //init done
	} else {
#if HAVE_DBUS
		if (rf->bus != NULL)
			dbus_connection_unref (rf->bus);
#endif
		abort_all_soup();
		printf("Plugin disabled\n");
	}

	return 0;
}


#if (EVOLUTION_VERSION < 22900)
void e_plugin_lib_disable(EPluginLib *ep);
#else
void e_plugin_lib_disable(EPlugin *ep);
#endif

void
#if (EVOLUTION_VERSION < 22900)
e_plugin_lib_disable(EPluginLib *ep)
#else
e_plugin_lib_disable(EPlugin *ep)
#endif
{
	g_print("DIE!\n");
}

void
free_filter_uids (gpointer user_data, GObject *ex_msg)
{
	g_print("weak unref called on filter_uids\n");
}

void
create_mail(create_feed *CF)
{
	CamelFolder *mail_folder;
	CamelMimeMessage *new = camel_mime_message_new();
	CamelInternetAddress *addr;
	CamelMessageInfo *info;
	CamelException *ex = NULL;
	struct tm tm;
	time_t time, actual_time;
	CamelDataWrapper *rtext;
	CamelContentType *type;
	CamelStream *stream;
	char *appended_uid = NULL;
	gchar *author = CF->q ? CF->q : CF->sender;
	gchar *tmp, *tmp2, *safe_subj;
	CamelMimePart *part, *msgp;
	CamelMultipart *mp;
	GString *cats;
	GList *p, *l;
	gchar *time_str, *buf;
	gint offset;

	mail_folder = check_feed_folder(CF->full_path);
#if (DATASERVER_VERSION >= 2031001)
	g_object_ref(mail_folder);
#else
	camel_object_ref(mail_folder);
#endif

	info = camel_message_info_new(NULL);
	camel_message_info_set_flags(info, CAMEL_MESSAGE_SEEN, 1);

	tmp = decode_entities(CF->subj);
	tmp2 = markup_decode(tmp);
	safe_subj = camel_header_encode_string((unsigned char *)tmp2);
	g_strdelimit(safe_subj, "\n", ' ');
	camel_mime_message_set_subject(new, safe_subj);
	g_free(tmp);
	g_free(tmp2);

	addr = camel_internet_address_new();
	d("date:%s\n", CF->date);
	camel_address_decode((CamelAddress *)addr, author);
	camel_mime_message_set_from(new, addr);
#if (DATASERVER_VERSION >= 2031001)
	g_object_unref(addr);
#else
	camel_object_unref(addr);
#endif

	offset = 0;
	actual_time = CAMEL_MESSAGE_DATE_CURRENT;

	//handle pubdate
	if (CF->date) {
		//check if CF->date obeys rfc822
		if (!is_rfc822(CF->date))
			camel_mime_message_set_date(
				new,
				CAMEL_MESSAGE_DATE_CURRENT,
				0);
		else {
			actual_time = camel_header_decode_date(
					CF->date, &offset);
			camel_mime_message_set_date(
					new,
					actual_time,
					offset);
		}
	} else {
		if (CF->dcdate)	{ //dublin core
			d("dcdate:%s\n", CF->dcdate);
			if (strptime(CF->dcdate, "%Y-%m-%dT%T%z", &tm)) {
				time = mktime(&tm);
				actual_time = camel_header_decode_date (
						ctime(&time), &offset);
			}
		}
		/*use 'now' as time for failsafe*/
		d("using now() as fallback\n");
		camel_mime_message_set_date(new, actual_time, offset);
	}
	time = camel_mime_message_get_date (new, NULL) ;
	time_str = asctime(gmtime(&time));
	buf = g_strdup_printf(
			"from %s by localhost via evolution-rss-%s with libsoup-%d; %s\r\n",
			"RSS",
			VERSION,
			LIBSOUP_VERSION,
			time_str);
	camel_medium_set_header(CAMEL_MEDIUM(new), "Received", buf);
	camel_medium_set_header(CAMEL_MEDIUM(new), "Website", CF->website);
	camel_medium_set_header(CAMEL_MEDIUM(new), "RSS-ID", CF->feedid);
	camel_medium_set_header(
			CAMEL_MEDIUM(new),
			"X-evolution-rss-feed-ID",
			g_strstrip(CF->feed_uri));
	if (CF->comments)
		camel_medium_set_header(
			CAMEL_MEDIUM(new),
			"X-evolution-rss-comments",
			CF->comments);
	if (CF->category) {
		cats = g_string_new(NULL);
		for (p = (GList *)CF->category; p != NULL; p=p->next) {
			if (p->next)
				g_string_append_printf(
					cats, "%s, ", (char *)p->data);
			else
				g_string_append_printf(
					cats, "%s", (char *)p->data);
		}
		camel_medium_set_header(
			CAMEL_MEDIUM(new),
			"X-evolution-rss-category",
			cats->str);
		g_string_free(cats, TRUE);
	}
	rtext = camel_data_wrapper_new ();
	type = camel_content_type_new ("x-evolution", "evolution-rss-feed");
	camel_content_type_set_param (type, "format", "flowed");
	camel_data_wrapper_set_mime_type_field (rtext, type);
	camel_content_type_unref (type);
	stream = camel_stream_mem_new ();
	// w/out an format argument this throws and seg fault
	camel_stream_printf (stream, "%s", CF->body);
	camel_data_wrapper_construct_from_stream (rtext, stream);
#if (DATASERVER_VERSION >= 2031001)
	g_object_unref (stream);
#else
	camel_object_unref (stream);
#endif

	if (CF->attachedfiles) {
			mp = camel_multipart_new();
			camel_multipart_set_boundary(mp, NULL);

			part = camel_mime_part_new();
#if EVOLUTION_VERSION >= 23100
			camel_medium_set_content(
				(CamelMedium *)part, (CamelDataWrapper *)rtext);
#else
			camel_medium_set_content_object(
				(CamelMedium *)part, (CamelDataWrapper *)rtext);
#endif
			camel_multipart_add_part(mp, part);
#if (DATASERVER_VERSION >= 2031001)
			g_object_unref(part);
#else
			camel_object_unref(part);
#endif
		for (l = g_list_first(CF->attachedfiles); l != NULL; l = l->next) {
			msgp = file_to_message(l->data);
			if (msgp) {
				camel_multipart_add_part(mp, msgp);
#if (DATASERVER_VERSION >= 2031001)
				g_object_unref(msgp);
#else
				camel_object_unref(msgp);
#endif
			}
		}
#if EVOLUTION_VERSION >= 23100
		camel_medium_set_content((CamelMedium *)new, (CamelDataWrapper *)mp);
#else
		camel_medium_set_content_object((CamelMedium *)new, (CamelDataWrapper *)mp);
#endif
#if (DATASERVER_VERSION >= 2031001)
		g_object_unref(mp);
#else
		camel_object_unref(mp);
#endif
	} else	if (CF->encl) {
		mp = camel_multipart_new();
		camel_multipart_set_boundary(mp, NULL);

		part = camel_mime_part_new();
#if EVOLUTION_VERSION >= 23100
		camel_medium_set_content(
			(CamelMedium *)part, (CamelDataWrapper *)rtext);
#else
		camel_medium_set_content_object(
			(CamelMedium *)part, (CamelDataWrapper *)rtext);
#endif

		camel_multipart_add_part(mp, part);
#if (DATASERVER_VERSION >= 2031001)
		g_object_unref(part);
#else
		camel_object_unref(part);
#endif
		msgp = file_to_message(CF->encl);
		if (msgp) {
			camel_multipart_add_part(mp, msgp);
#if (DATASERVER_VERSION >= 2031001)
			g_object_unref(msgp);
#else
			camel_object_unref(msgp);
#endif
		}
#if EVOLUTION_VERSION >= 23100
		camel_medium_set_content((CamelMedium *)new, (CamelDataWrapper *)mp);
#else
		camel_medium_set_content_object((CamelMedium *)new, (CamelDataWrapper *)mp);
#endif
#if (DATASERVER_VERSION >= 2031001)
		g_object_unref(mp);
#else
		camel_object_unref(mp);
#endif
	} else
#if EVOLUTION_VERSION >= 23100
		camel_medium_set_content(CAMEL_MEDIUM(new), CAMEL_DATA_WRAPPER(rtext));
#else
		camel_medium_set_content_object(CAMEL_MEDIUM(new), CAMEL_DATA_WRAPPER(rtext));
#endif

	camel_folder_append_message(mail_folder, new, info, &appended_uid, ex);

	/* no point in filtering mails at import time as it just
	 * wastes time, user can setup his own afterwards
	 */
	if (appended_uid != NULL
		&& !rf->import
		&& !CF->encl
		&& !g_list_length(CF->attachments)) {	//do not filter enclosure at this time nor media files
//		g_warning("FILTER DISABLED\n");
		filter_uids = g_ptr_array_sized_new(1);
		g_ptr_array_add(filter_uids, appended_uid);
		mail_filter_on_demand (mail_folder, filter_uids);
/* FIXME do not know how to free this */
//		g_object_weak_ref((GObject *)filter_uids, free_filter_uids, NULL);
	}
	//FIXME too lasy to write a separate function
	if (!rf->import)
		mail_refresh_folder(mail_folder, NULL, NULL);
#if (DATASERVER_VERSION >= 2031001)
	g_object_unref(rtext);
	g_object_unref(new);
	g_object_unref(mail_folder);
#else
	camel_object_unref(rtext);
	camel_object_unref(new);
	camel_object_unref(mail_folder);
#endif
	camel_message_info_free(info);
	g_free(buf);
}

/************ RDF Parser *******************/

gchar *
get_real_channel_name(gchar *uri, gchar *failed)
{
	gpointer crc_feed = gen_md5(uri);
	gchar *chn_name = g_hash_table_lookup(rf->hrname_r, crc_feed);
	g_free(crc_feed);
	return chn_name ? chn_name : failed;
}

CamelMimePart *
file_to_message(const char *filename)
{
	const char *type;
	CamelStreamFs *file;
	CamelMimePart *msg = camel_mime_part_new();
	CamelDataWrapper *content;
	gchar *tname;

	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (g_file_test(filename, G_FILE_TEST_IS_REGULAR), NULL);

	camel_mime_part_set_encoding(msg, CAMEL_TRANSFER_ENCODING_BINARY);
	content = camel_data_wrapper_new();

	file = (CamelStreamFs *)
			camel_stream_fs_new_with_name(filename,
				O_RDWR|O_CREAT,
				0666);

	if (!file)
		return NULL;

	camel_data_wrapper_construct_from_stream(content, (CamelStream *)file);
#if (DATASERVER_VERSION >= 2031001)
	g_object_unref((CamelObject *)file);
#else
	camel_object_unref((CamelObject *)file);
#endif
#if EVOLUTION_VERSION >= 23100
	camel_medium_set_content((CamelMedium *)msg, content);
#else
	camel_medium_set_content_object((CamelMedium *)msg, content);
#endif
#if (DATASERVER_VERSION >= 2031001)
	g_object_unref(content);
#else
	camel_object_unref(content);
#endif

#if EVOLUTION_VERSION < 22900
	type = em_utils_snoop_type(msg);
#else
	type = em_format_snoop_type(msg);
#endif
	if (type)
		camel_data_wrapper_set_mime_type((CamelDataWrapper *)msg, type);

	tname = g_path_get_basename(filename);
	camel_mime_part_set_filename(msg, tname);
	g_free(tname);

	return msg;
}

void
free_cf(create_feed *CF)
{
	g_free(CF->full_path);
	g_free(CF->q);
	g_free(CF->sender);
	g_free(CF->subj);
	g_free(CF->body);
	g_free(CF->date);
	g_free(CF->dcdate);
	g_free(CF->website);
	g_free(CF->feedid);
	g_free(CF->encl);
	g_free(CF->enclurl);
	g_free(CF->feed_fname);
	g_free(CF->feed_uri);
	if (CF->comments)
		g_free(CF->comments);
	if (CF->category) {
		g_list_foreach(CF->category, (GFunc)g_free, NULL);
		g_list_free(CF->category);
	}
	if (CF->attachments) {
		g_list_foreach(CF->attachments, (GFunc)g_free, NULL);
		g_list_free(CF->attachments);
	}
	if (CF->attachedfiles) {
		g_list_foreach(CF->attachedfiles, (GFunc)g_free, NULL);
		g_list_free(CF->attachedfiles);
	}
	g_free(CF);
}

typedef struct CFL {
	gchar *url;
	FILE *file;
	create_feed *CF;
} cfl;

void
#if LIBSOUP_VERSION < 2003000
finish_attachment (
	SoupMessage *msg,
	cfl *user_data);
#else
finish_attachment (
	SoupSession *soup_sess,
	SoupMessage *msg,
	cfl *user_data);
#endif

void
process_attachments(create_feed *CF)
{
	cfl *CFL;
	GList *l = g_list_first(CF->attachments);

	g_return_if_fail(CF->attachments != NULL);

	do {
		gchar *tmpdir, *name;

		if (!strlen(l->data))
			continue;
		if (g_list_find_custom(rf->enclist, l->data,
			(GCompareFunc)strcmp))
			continue;
		tmpdir = e_mkdtemp("evo-rss-XXXXXX");
		if ( tmpdir == NULL)
			continue;
		name = g_build_filename(tmpdir,
			g_path_get_basename(l->data),
			NULL);
		g_free(tmpdir);
		CFL = g_new0(cfl, 1);
		CFL->url = l->data;
		CFL->CF = CF;
		d("enclosure file:%s\n", name)
		CF->attachedfiles = g_list_append(CF->attachedfiles, name);
		CF->attachmentsqueue++;
		CFL->file = fopen(name, "w");
		if (!CFL->file) return;
		download_unblocking(
			CFL->url,
			download_chunk,
			CFL->file,
			(gpointer)finish_attachment,
			CFL,
			0,
			NULL);
	} while ((l = l->next));
}



#if LIBSOUP_VERSION < 2003000
void
finish_attachment (SoupMessage *msg,
		cfl *user_data)
#else
void
finish_attachment (SoupSession *soup_sess,
		SoupMessage *msg,
		cfl *user_data)
#endif
{
#if LIBSOUP_VERSION < 2003000
	fwrite(msg->response.body,
		msg->response.length,
		1,
		user_data->file);
#else
	fwrite(msg->response_body->data,
		msg->response_body->length,
		1,
		user_data->file);
#endif
	fclose(user_data->file);

	rf->enclist = g_list_remove(rf->enclist, user_data->url);
	//g_free(msg->response_body->data);
	//g_object_unref(msg);
	if (user_data->CF->attachmentsqueue)
		user_data->CF->attachmentsqueue--;

	if (!user_data->CF->attachmentsqueue) {
		if (!feed_is_new(user_data->CF->feed_fname, user_data->CF->feed_uri)) {
			create_mail(user_data->CF);
			write_feed_status_line(
				user_data->CF->feed_fname,
				user_data->CF->feed_uri);
			free_cf(user_data->CF);
		}
	}
	g_free(user_data);

	if (net_queue_run_count) net_queue_run_count--;
	if (!net_qid)
		net_qid = g_idle_add(
				(GSourceFunc)net_queue_dispatcher,
				NULL);
}

void
process_enclosure(create_feed *CF)
{
	gchar *tmpdir, *name;

	if (g_list_find_custom(rf->enclist, CF->encl,
			(GCompareFunc)strcmp))
		return;
	tmpdir = e_mkdtemp("evo-rss-XXXXXX");
	if ( tmpdir == NULL)
		return;
	name = g_build_filename(tmpdir,
		g_path_get_basename(CF->encl),
		NULL);
	g_free(tmpdir);
	CF->enclurl = CF->encl;
	CF->encl = name;
	d("enclosure file:%s\n", name)
	CF->efile = fopen(name, "w");
	if (!CF->efile) return;
	download_unblocking(
		CF->enclurl,
		download_chunk,
		CF->efile,
		(gpointer)finish_enclosure,
		CF,
		0,
		NULL);
}

void
#if LIBSOUP_VERSION < 2003000
finish_enclosure (SoupMessage *msg,
		create_feed *user_data)
#else
finish_enclosure (SoupSession *soup_sess,
		SoupMessage *msg,
		create_feed *user_data)
#endif
{
#if LIBSOUP_VERSION < 2003000
	fwrite(msg->response.body,
		msg->response.length,
		1,
		user_data->efile);
#else
	fwrite(msg->response_body->data,
		msg->response_body->length,
		1,
		user_data->efile);
#endif
	fclose(user_data->efile);

	if (!feed_is_new(user_data->feed_fname, user_data->feed_uri)) {
		create_mail(user_data);
		write_feed_status_line(
				user_data->feed_fname,
				user_data->feed_uri);
	}
	rf->enclist = g_list_remove(rf->enclist, user_data->enclurl);
	free_cf(user_data);
	//g_free(msg->response_body->data);
	//g_object_unref(msg);
	if (net_queue_run_count) net_queue_run_count--;
	if (!net_qid)
		net_qid = g_idle_add(
				(GSourceFunc)net_queue_dispatcher,
				NULL);
}

gchar *update_comments(RDF *r);

gchar *
update_comments(RDF *r)
{
	guint i;
	create_feed *CF;
	xmlNodePtr el;
	gchar *scomments;
	GString *comments = g_string_new(NULL);
	for (i=0; NULL != (el = g_array_index(r->item, xmlNodePtr, i)); i++) {
		CF = parse_channel_line(el->children, NULL, NULL, NULL);
		g_string_append_printf (comments,
			"<div style=\"border: solid #%06x 1px; background-color: #%06x; padding: 0px; color: #%06x;\">",
			frame_colour & 0xffffff,
			content_colour & 0xEDECEB & 0xffffff,
			text_colour & 0xffffff);
		g_string_append_printf (comments,
			"<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
			"<a href=%s><b>%s</b></a> on %s</div>",
			content_colour & 0xEDECEB & 0xffffff,
			text_colour & 0xffffff,
			CF->website, CF->subj, CF->date);
		g_string_append_printf (comments,
			"<div style=\"border: solid #%06x 0px; background-color: #%06x; padding: 10px; color: #%06x;\">"
			"%s</div>",
			frame_colour & 0xffffff,
			content_colour & 0xffffff,
			text_colour & 0xffffff,
				CF->body);
		g_string_append_printf(comments, "</div>&nbsp;");
		free_cf(CF);
	}
	commcnt=i;
	scomments=comments->str;
	g_string_free(comments, FALSE);
	return scomments;
}

gchar *
display_comments (RDF *r, EMFormatHTML *format)
{
	gchar *tmp;
	xmlNodePtr root = xmlDocGetRootElement (r->cache);
	if (tree_walk (root, r)) {
		gchar *comments = update_comments(r);
		tmp = process_images(comments, r->uri, TRUE, format);
		g_free(comments);
		if (r->maindate)
			g_free(r->maindate);
		g_array_free(r->item, TRUE);
		g_free(r->cache);
		if (r->type)
			g_free(r->type);
		if (r)
			g_free(r);
		return tmp;
	}
	return NULL;
}

gchar *
process_feed(RDF *r)
{
	xmlNodePtr root = xmlDocGetRootElement (r->cache);
	if (tree_walk (root, r)) {
		update_feed_image(r);
		return r->title;
	}
	return NULL;
}

gboolean
display_feed(RDF *r)
{
	r->feedid = update_channel(r);
	if (r->maindate)
		g_free(r->maindate);
	g_array_free(r->item, TRUE);
	g_free(r->feedid);
	return 0;
}

gchar *
display_doc (RDF *r)
{
	gchar *title = NULL;
	if ((title = process_feed(r)))
		display_feed(r);
	return title;
}

void delete_oldest_article(CamelFolder *folder, guint unread);

void
delete_oldest_article(CamelFolder *folder, guint unread)
{
	CamelMessageInfo *info;
	GPtrArray *uids;
	guint i, j = 0, imax = 0;
	guint q = 0;
	guint32 flags;
	time_t date, min_date = 0;
	uids = camel_folder_get_uids (folder);
	for (i = 0; i < uids->len; i++) {
		info = camel_folder_get_message_info(folder, uids->pdata[i]);
		if (info) {
			if (rf->current_uid && !strcmp(rf->current_uid, uids->pdata[i]))
				goto out;
			date = camel_message_info_date_sent(info);
			if (!date)
				goto out;
			flags = camel_message_info_flags(info);
			if (flags & CAMEL_MESSAGE_FLAGGED)
				goto out;
			if (flags & CAMEL_MESSAGE_DELETED)
				goto out;
			if (flags & CAMEL_MESSAGE_SEEN) {
				if (!j) {
					min_date = date;
					imax = i;
					j++;
				}
				if (date < min_date) {
					imax = i;
					min_date = date;
				}
			} else {		//UNSEEN
				if (unread) {
					if (!q) {
						min_date = date;
						imax = i;
						q++;
					}
					if (date < min_date) {
						imax = i;
						min_date = date;
					}
				}
			}
		}
		d("uid:%d j:%d/%d, date:%s, imax:%d\n",
			i, j, q, ctime(&min_date), imax);
out:		camel_message_info_free(info);
	}
	camel_folder_freeze(folder);
	if (min_date) {
		camel_folder_delete_message (folder, uids->pdata[imax]);
	}
	camel_folder_thaw(folder);
	while (gtk_events_pending())
		gtk_main_iteration ();
	camel_folder_free_uids (folder, uids);
}

void
get_feed_age(RDF *r, gpointer name)
{
	CamelMessageInfo *info;
	CamelFolder *folder;
	CamelStore *store = rss_component_peek_local_store();
	CamelMimeMessage *message;
	GPtrArray *uids;
	time_t date, now;
	guint i,j,total;
	guint32 flags;
	gpointer key = lookup_key(name);
	gchar *el, *feedid;
	gchar *real_name;
	gboolean match;
	guint del_unread, del_notpresent, del_feed;

	gchar *real_folder = lookup_feed_folder(name);
	d("Cleaning folder: %s\n", real_folder);

	real_name = g_strdup_printf(
			"%s" G_DIR_SEPARATOR_S "%s",
			lookup_main_folder(),
			real_folder);
	if (!(folder = camel_store_get_folder (store, real_name, 0, NULL)))
		goto fail;
	time (&now);

	del_unread = GPOINTER_TO_INT(
		g_hash_table_lookup(rf->hrdel_unread, key));
	del_notpresent = GPOINTER_TO_INT(
		g_hash_table_lookup(rf->hrdel_notpresent, key));
	del_feed = GPOINTER_TO_INT(
		g_hash_table_lookup(rf->hrdel_feed, key));
	inhibit_read = 1;
	if (del_notpresent) {
		uids = camel_folder_get_uids (folder);
		camel_folder_freeze(folder);
		for (i = 0; i < uids->len; i++) {
			el = NULL;
			match = FALSE;
			message = camel_folder_get_message(folder, uids->pdata[i], NULL);
			if (message == NULL)
				break;
			feedid  = (gchar *)camel_medium_get_header (
					CAMEL_MEDIUM(message),
					"X-Evolution-Rss-Feed-id");
			if (!r->uids) {
#if (DATASERVER_VERSION >= 2031001)
				g_object_unref (message);
#else
				camel_object_unref (message);
#endif
				break;
			}

			for (j=0; NULL != (el = g_array_index(r->uids, gpointer, j)); j++) {
				if (!g_ascii_strcasecmp(g_strstrip(feedid), g_strstrip(el))) {
					match = TRUE;
					break;
				}
			}
			if (!match) {
//				g_print("info\n");
				info = camel_folder_get_message_info(folder, uids->pdata[i]);
//				g_print("info done\n");
				flags = camel_message_info_flags(info);
				if ((del_unread) && !(flags & CAMEL_MESSAGE_FLAGGED)) {
					gchar *feed_dir, *feed_name;
					camel_folder_delete_message(folder, uids->pdata[i]);
					feed_dir = rss_component_peek_base_directory();
					feed_name = g_build_path(G_DIR_SEPARATOR_S, feed_dir, key, NULL);
					g_free(feed_dir);
					feed_remove_status_line(
						feed_name,
						feedid);
					g_free(feed_name);
				}
				camel_folder_free_message_info(folder, info);
			}
#if (DATASERVER_VERSION >= 2031001)
			g_object_unref (message);
#else
			camel_object_unref (message);
#endif
		}
		camel_folder_free_uids (folder, uids);
		camel_folder_sync (folder, TRUE, NULL);
		camel_folder_thaw(folder);
		camel_folder_expunge (folder, NULL);
	}
	if (del_feed == 2) {
		guint del_days = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_days, key));
		uids = camel_folder_get_uids (folder);
		camel_folder_freeze(folder);
		for (i = 0; i < uids->len; i++) {
			info = camel_folder_get_message_info(folder, uids->pdata[i]);
			if (info == NULL)
				continue;
			if (rf->current_uid && strcmp(rf->current_uid, uids->pdata[i])) {
				date = camel_message_info_date_sent(info);
				if (date < now - del_days * 86400) {
					flags = camel_message_info_flags(info);
					if (!(flags & CAMEL_MESSAGE_SEEN)) {
						if ((del_unread) && !(flags & CAMEL_MESSAGE_FLAGGED)) {
							//camel_message_info_set_flags(info, CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_DELETED, ~0);
							camel_folder_delete_message(folder, uids->pdata[i]);
						}
					} else
						if (!(flags & CAMEL_MESSAGE_FLAGGED)) {
							//camel_message_info_set_flags(info, CAMEL_MESSAGE_DELETED, ~0);
							camel_folder_delete_message(folder, uids->pdata[i]);
						}
				}
			}
			camel_folder_free_message_info(folder, info);
		}
		camel_folder_free_uids (folder, uids);
		camel_folder_sync (folder, TRUE, NULL);
		camel_folder_thaw(folder);
		camel_folder_expunge (folder, NULL);
	}
	if (del_feed == 1) {
		guint del_messages = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_messages, key));
		guint total = camel_folder_get_message_count(folder);
		i=1;
		while (del_messages < camel_folder_get_message_count(folder)
			- camel_folder_get_deleted_message_count(folder) && i <= total) {
			delete_oldest_article(folder, del_unread);
			i++;
		}
		camel_folder_sync (folder, TRUE, NULL);
		camel_folder_expunge (folder, NULL);
	}
	total = camel_folder_get_message_count (folder);
#if (DATASERVER_VERSION >= 2031001)
	g_object_unref (folder);
#else
	camel_object_unref (folder);
#endif
	d("delete => remaining total:%d\n", total);
fail:	g_free(real_name);
	inhibit_read = 0;
}

