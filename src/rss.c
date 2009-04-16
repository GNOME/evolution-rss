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

#ifdef HAVE_CONFIG_H

#include "config.h"
#endif

int rss_verbose_debug = 0;
#define d(x) (rss_verbose_debug?(x):0)

#include <string.h>
#include <stdio.h>
#include <time.h>

#include <camel/camel-mime-message.h>
#include <camel/camel-folder.h>
#include <camel/camel-exception.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-fs.h>

#include <mail/em-popup.h>
#include <e-util/e-error.h>
#include <e-util/e-icon-factory.h>
#include <e-util/e-mktemp.h>

#include <mail/em-config.h>

#ifdef EVOLUTION_2_12
#include <mail/em-event.h>
#endif

#include <mail/em-utils.h>
#include <mail/em-folder-tree.h>
#include <mail/em-folder-tree-model.h>
#include <mail/em-folder-utils.h>
#include <mail/em-folder-view.h>
#include <mail/mail-mt.h>
#include <mail/mail-component.h>
#include <mail/mail-tools.h>

#include <misc/e-activity-handler.h>

#include <mail/em-format-html.h>

#include <mail/em-format.h>
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <bonobo/bonobo-shlib-factory.h>

#include <glade/glade-xml.h>
#include <glade/glade.h>
#include <shell/evolution-config-control.h>
#include <shell/e-component-view.h>///
#include <shell/es-event.h>
#include <camel/camel-data-cache.h>
#include <camel/camel-file-utils.h>

#include <libxml/parserInternals.h>
#include <libxml/xmlmemory.h>
#include <libxml/HTMLparser.h>

#ifdef HAVE_RENDERKIT
#ifdef HAVE_GECKO
#ifdef HAVE_GECKO_1_9
#include <gtkmozembed.h>
#else
#include <gtkembedmoz/gtkmozembed.h>
#endif
#endif
#include "gecko-utils.h"

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

#include <errno.h>

#include <libsoup/soup.h>
#if LIBSOUP_VERSION < 2003000
#include <libsoup/soup-message-queue.h>
#endif

#include "rss.h"
#include "parser.h"
#include "network-soup.c"
#include "file-gio.c"
#include "fetch.c"
#include "misc.c"
#if HAVE_DBUS
#include "dbus.c"
#endif
#include "rss-config-factory.c"
#include "rss-icon-factory.c"
#include "parser.c"

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

static CamelDataCache *http_cache;

static volatile int org_gnome_rss_controls_counter_id = 0;

struct _org_gnome_rss_controls_pobject {
        EMFormatHTMLPObject object;

        CamelMimePart *part;
        EMFormatHTML *format;
	GtkWidget *html;
	GtkWidget *container;
	CamelStream *stream;
	GtkWidget *mozembedwindow;	//window containing GtkMozEmbed
	gchar *website;
	guint is_html;
	gchar *mem;
	guint shandler;		//mycall handler_id
	guint counter;		//general counter for carring various numbers
};

GtkWidget *evo_window;
static GdkPixbuf *folder_icon;
GHashTable *icons = NULL;
gchar *pixfile;
char *pixfilebuf;
gsize pixfilelen;
extern int xmlSubstituteEntitiesDefaultValue;

rssfeed *rf = NULL;
gboolean inhibit_read = FALSE;	//prevent mail selection when deleting folder
gboolean delete_op = FALSE;	//delete in progress
gchar *commstream = NULL; 	//global comments stream
guint commcnt = 0; 	//global number of comments
gchar *commstatus = "";
guint32 frame_colour;
guint32 content_colour;
guint32 text_colour;
gboolean browser_fetching = 0; //mycall event could be triggered many times in first step (fetching)
gint browser_fill = 0;	//how much data currently written to browser

gboolean setup_feed(add_feed *feed);
gchar *display_doc (RDF *r);
gchar *display_comments (RDF *r);
void check_folders(void);
gchar *strplchr(gchar *source);
static char *gen_md5(gchar *buffer);
CamelMimePart *file_to_message(const char *name);
void save_gconf_feed(void);
void check_feed_age(void);
static gboolean check_if_match (gpointer key, gpointer value, gpointer user_data);
static void del_days_cb (GtkWidget *widget, add_feed *data);
static void del_messages_cb (GtkWidget *widget, add_feed *data);
void get_feed_age(gpointer key, gpointer value);
gboolean cancel_soup_sess(gpointer key, gpointer value, gpointer user_data);
void abort_all_soup(void);
gchar *encode_html_entities(gchar *str);
static void
#if LIBSOUP_VERSION < 2003000
finish_image (SoupMessage *msg, CamelStream *user_data);
#else
finish_image (SoupSession *soup_sess, SoupMessage *msg, CamelStream *user_data);
#endif
static void
#if LIBSOUP_VERSION < 2003000
finish_create_image (SoupMessage *msg, gchar *user_data);
#else
finish_create_image (SoupSession *soup_sess, SoupMessage *msg, gchar *user_data);
#endif
gchar *get_main_folder(void);
void fetch_comments(gchar *url, CamelStream *stream);

struct _MailComponentPrivate {
        GMutex *lock;

        /* states/data used during shutdown */
        enum { MC_QUIT_START, MC_QUIT_SYNC, MC_QUIT_THREADS } quit_state;
        int quit_count;
        int quit_expunge;       /* expunge on quit this time around? */

        char *base_directory;

        EMFolderTreeModel *model;

//        EActivityHandler *activity_handler;

        MailAsyncEvent *async_event;
        GHashTable *store_hash; /* stores store_info objects by store */

//        RuleContext *search_context;

        char *context_path;     /* current path for right-click menu */

        CamelStore *local_store;

        EComponentView *component_view;
};
static void
dialog_key_destroy (GtkWidget *widget, gpointer data);
guint fallback_engine(void);

gchar *
decode_entities(gchar *source);

struct _rfMessage {
	guint 	 status_code;
	gchar 	*body;
	goffset	 length;
};

typedef struct _rfMessage rfMessage;

void generic_finish_feed(rfMessage *msg, gpointer user_data);
gchar *print_comments(gchar *url, gchar *stream);
static void refresh_cb (GtkWidget *button, EMFormatHTMLPObject *pobject);

/*======================================================================*/

gpointer
lookup_key(gpointer key)
{
	return g_hash_table_lookup(rf->hrname, key);
}

/* hash table of ops->dialogue of active errors */
static GHashTable *active_errors = NULL;

static void error_destroy(GtkObject *o, void *data)
{
        g_hash_table_remove(active_errors, data);
}

static void error_response(GtkObject *o, int button, void *data)
{
        gtk_widget_destroy((GtkWidget *)o);
}

void
rss_error(gpointer key, gchar *name, gchar *error, gchar *emsg)
{
	GtkWidget *ed;
	gchar *msg;

	if (name)
               	msg = g_strdup_printf("\n%s\n%s", name, emsg);
	else
		msg = g_strdup(emsg); 

#if (EVOLUTION_VERSION >= 22200)
	if (key)
	{ 
		if (!g_hash_table_lookup(rf->error_hash, key))
		{
        		EActivityHandler *activity_handler = mail_component_peek_activity_handler (mail_component_peek());
//			guint activity_id = g_hash_table_lookup(rf->activity, key);
                	ed  = e_error_new(NULL, "org-gnome-evolution-rss:feederr",
                	             error, msg, NULL);
			gpointer newkey = g_strdup(key);
                	g_signal_connect(ed, "response", G_CALLBACK(err_destroy), NULL);
                	g_signal_connect(ed, "destroy", G_CALLBACK(dialog_key_destroy), newkey);
#if (EVOLUTION_VERSION >= 22300)
        		guint id = e_activity_handler_make_error (activity_handler, (char *)mail_component_peek(), E_LOG_ERROR, ed);
#else
        		guint id = e_activity_handler_make_error (activity_handler, (char *)mail_component_peek(), msg, ed);
#endif
			g_hash_table_insert(rf->error_hash, newkey, GINT_TO_POINTER(id));
		}
/*		taskbar_op_finish(key);*/
		goto out;
	}
#endif

	if (!rf->errdialog)
        {

                ed  = e_error_new(NULL, "org-gnome-evolution-rss:feederr",
                             error, msg, NULL);
                g_signal_connect(ed, "response", G_CALLBACK(err_destroy), NULL);
                gtk_widget_show(ed);
                rf->errdialog = ed;
	}

out:    g_free(msg);
}

void
cancel_active_op(gpointer key)
{
	gpointer key_session = g_hash_table_lookup(rf->key_session, key); 
	gpointer value = g_hash_table_lookup(rf->session, key_session); 
	if (value)
		cancel_soup_sess(key_session, value, NULL);
}

void
taskbar_push_message(gchar *message)
{
	EActivityHandler *activity_handler = mail_component_peek_activity_handler (mail_component_peek ());
	e_activity_handler_set_message(activity_handler, message);
}

void
taskbar_pop_message(void)
{
	EActivityHandler *activity_handler = mail_component_peek_activity_handler (mail_component_peek ());
	e_activity_handler_unset_message(activity_handler);
}

guint
#if (EVOLUTION_VERSION >= 22200)
taskbar_op_new(gchar *message, gpointer key)
#else
taskbar_op_new(gchar *message)
#endif
{
	EActivityHandler *activity_handler = mail_component_peek_activity_handler (mail_component_peek ());
	char *mcp = g_strdup_printf("%p", mail_component_peek());
	static GdkPixbuf *progress_icon;
	guint activity_id = 
#if (EVOLUTION_VERSION >= 22306)
		e_activity_handler_cancelable_operation_started(activity_handler, "evolution-mail",
						message, TRUE,
						(void (*) (gpointer))abort_all_soup,
						 key);
#else 
	progress_icon = e_icon_factory_get_icon ("mail-unread", E_ICON_SIZE_MENU);
#if (EVOLUTION_VERSION >= 22200)
		e_activity_handler_cancelable_operation_started(activity_handler, "evolution-mail",
						progress_icon, message, TRUE,
						(void (*) (gpointer))abort_all_soup,
						 key);
#else
		e_activity_handler_operation_started(activity_handler, mcp,
						progress_icon, message, FALSE);
#endif
#endif

	g_free(mcp);
	return activity_id;
}

void
taskbar_op_set_progress(gpointer key, gchar *msg, gdouble progress)
{
	EActivityHandler *activity_handler = mail_component_peek_activity_handler (mail_component_peek ());
	guint activity_id = GPOINTER_TO_INT(g_hash_table_lookup(rf->activity, key));

	if (activity_id)
	{
		e_activity_handler_operation_progressing(activity_handler,
				activity_id,
                                g_strdup(msg), 
                                progress);
	}
}

void
taskbar_op_finish(gpointer key)
{
	EActivityHandler *activity_handler = mail_component_peek_activity_handler (mail_component_peek ());
	
	if (rf->activity)
	{
		guint activity_key = GPOINTER_TO_INT(g_hash_table_lookup(rf->activity, key));
		if (activity_key)
			e_activity_handler_operation_finished(activity_handler, activity_key);
		g_hash_table_remove(rf->activity, key);
	}
}

void
taskbar_op_message(void)
{
		gchar *tmsg = g_strdup_printf(_("Fetching Feeds (%d enabled)"), g_hash_table_size(rf->hrname));
#if (EVOLUTION_VERSION >= 22200)
		guint activity_id = taskbar_op_new(tmsg, "main");
#else
		guint activity_id = taskbar_op_new(tmsg);
#endif
		g_hash_table_insert(rf->activity, "main", GUINT_TO_POINTER(activity_id));
		g_free(tmsg);
}

static void
statuscb(NetStatusType status, gpointer statusdata, gpointer data)
{
//	rssfeed *rf = data;
    NetStatusProgress *progress;
    float fraction = 0;
    d(g_print("status:%d\n", status));

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
		if (rf->cancel_all)
			break;
#ifndef EVOLUTION_2_12
		if (rf->progress_dialog  && 0 <= fraction && 1 >= fraction)
		{
			gtk_progress_bar_set_fraction((GtkProgressBar *)rf->progress_bar, fraction);
			gchar *what = g_strdup_printf(_("%2.0f%% done"), fraction*100);
			gtk_label_set_text(GTK_LABEL(rf->label), data);
	        	gtk_progress_bar_set_text((GtkProgressBar *)rf->progress_bar, what);
	        	g_free(what);
		}
#else
		if (rf->progress_bar && 0 <= fraction && 1 >= fraction)
			gtk_progress_bar_set_fraction((GtkProgressBar *)rf->progress_bar, fraction);
		if (rf->sr_feed)
		{
			gchar *furl = g_strdup_printf("<b>%s</b>: %s", _("Feed"), data);
			gtk_label_set_markup (GTK_LABEL (rf->sr_feed), furl);
			g_free(furl);
		}
#endif
        }
	//update individual progress if previous percetage has not changed
	if (rf->progress_bar && rf->feed_queue)
		gtk_progress_bar_set_fraction((GtkProgressBar *)rf->progress_bar, 
			((gfloat)((100-(rf->feed_queue*100/g_hash_table_size(rf->hrname))))/100));
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

static void
browser_write(gchar *string, gint length)
{
	gchar *str = string;
	//gchar *str = g_strdup("gezzzzzza\n\n\n");
	gint len = length;
	while (len > 0) {
	if (len > 4096) {
		gtk_moz_embed_append_data(GTK_MOZ_EMBED(rf->mozembed),
			str, 4096);
		str+=4096;
	}
	else
		gtk_moz_embed_append_data(GTK_MOZ_EMBED(rf->mozembed),
		str, len);
	len-=4096;
	}
}

static void
browsercb(NetStatusType status, gpointer statusdata, gint data)
{
    NetStatusProgress *progress = (NetStatusProgress*)statusdata;
    switch (status) {
    case NET_STATUS_PROGRESS:
//		g_print("chunk:%s\n", progress->chunk);
		g_print("\n\n\n--------------\n %d %s \n=============\n\n\n", progress->chunksize, progress->chunk);
		browser_write(progress->chunk, progress->chunksize);
		browser_fill+=progress->chunksize;
        break;
    default:
        g_warning("unhandled network status %d\n", status);
    }
}

static void
textcb(NetStatusType status, gpointer statusdata, gpointer data)
{
    NetStatusProgress *progress;
    float fraction = 0;
    switch (status) {
    case NET_STATUS_PROGRESS:
        progress = (NetStatusProgress*)statusdata;
        if (progress->current > 0 && progress->total > 0) {
	fraction = (float)progress->current / progress->total;
	d(g_print("%f.", fraction*100));
	}
	while (gtk_events_pending())
      		gtk_main_iteration ();
        break;
    default:
        g_warning("unhandled network status %d\n", status);
    }
}

gboolean
create_user_pass_dialog(gchar *url)
{
GtkWidget *dialog1;
  GtkWidget *dialog_vbox1;
  GtkWidget *table1;
  GtkWidget *label1;
  GtkWidget *label2;
  GtkWidget *username;
  GtkWidget *password;
  GtkWidget *dialog_action_area1;
  GtkWidget *cancelbutton1;
  GtkWidget *okbutton1;
  GtkWidget *checkbutton1;
  GtkWidget *vbox1;
  GtkWidget *container;
  GtkWidget *container2;
  guint resp;

 GtkWidget *widget;
        GtkWidget *action_area;
        GtkWidget *content_area;
  //      gint type = msg->flags & E_PASSWORDS_REMEMBER_MASK;
    //    guint noreply = msg->noreply;
        gboolean visible;
        AtkObject *a11y;

//        msg->noreply = 1;

	if (!rf->hruser)
		rf->hruser = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	if (!rf->hrpass)
		rf->hrpass = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

        widget = gtk_dialog_new_with_buttons (
                _("Enter User/Pass for feed"), NULL, 0,
                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                GTK_STOCK_OK, GTK_RESPONSE_OK,
                NULL);
        gtk_dialog_set_has_separator (GTK_DIALOG (widget), FALSE);
        gtk_dialog_set_default_response (
                GTK_DIALOG (widget), GTK_RESPONSE_OK);
        gtk_window_set_resizable (GTK_WINDOW (widget), FALSE);
//        gtk_window_set_transient_for (GTK_WINDOW (widget), msg->parent);
        gtk_window_set_position (GTK_WINDOW (widget), GTK_WIN_POS_CENTER_ON_PARENT);
        gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
        GtkWidget *password_dialog = GTK_DIALOG (widget);

        action_area = gtk_dialog_get_action_area (password_dialog);
        content_area = gtk_dialog_get_content_area (password_dialog);

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

	char *markup;
	markup = g_markup_printf_escaped (_("Enter your username and password for:\n '%s'"), url);
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
	gchar *user = g_hash_table_lookup(rf->hruser,  url);
	if (user)
		gtk_entry_set_text (GTK_ENTRY (username), user);

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
	gchar *pass = g_hash_table_lookup(rf->hrpass,  url);
	if (pass)
		gtk_entry_set_text (GTK_ENTRY (password), pass);

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
      //          msg->check = widget;

	gtk_table_attach (
                        GTK_TABLE (container), checkbutton1,
                        1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	gint result = gtk_dialog_run(GTK_DIALOG(password_dialog));
	switch (result)
	{
	case GTK_RESPONSE_OK:
        	if (user)
        	    g_hash_table_remove(rf->hruser, url);
        	g_hash_table_insert(rf->hruser, url, 
			g_strdup(gtk_entry_get_text (GTK_ENTRY (username))));
        	if (pass)
            		g_hash_table_remove(rf->hrpass, url);
        	g_hash_table_insert(rf->hrpass, url, 
			g_strdup(gtk_entry_get_text (GTK_ENTRY (password))));
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton1)))
			save_up(url);
		else
			del_up(url);
	
        	gtk_widget_destroy (password_dialog);
		resp = 0;
        	break;
    default:
        gtk_widget_destroy (password_dialog);
	resp = 1;
        break;
  }
	return resp;
}

gboolean
cancel_soup_sess(gpointer key, gpointer value, gpointer user_data)
{
#if LIBSOUP_VERSION < 2003000
	SoupUri *uri =  soup_message_get_uri((SoupMessage *)value);
#else
	SoupURI *uri =  soup_message_get_uri((SoupMessage *)value);
#endif
	d(g_print("cancel url:%s%s?%s\n", uri->host, uri->path, uri->query?uri->query:""));

	if (SOUP_IS_SESSION(key))
	{
/*		if (SOUP_IS_MESSAGE(value))
		{
#if LIBSOUP_VERSION < 2003000
			soup_message_set_status(value,  SOUP_STATUS_CANCELLED);
			soup_session_cancel_message(key, value);
#else
			soup_session_cancel_message(key, value, SOUP_STATUS_CANCELLED);
#endif
		}*/
		soup_session_abort(key);
		g_hash_table_find(rf->key_session,
                	remove_if_match,
                	user_data);
	}
	return TRUE;
}
void
remove_weak(gpointer key, gpointer value, gpointer user_data)
{
	g_object_weak_unref(value, unblock_free, key);
}

gboolean
timeout_soup(void)
{
	g_print("Network timeout occured. Cancel active operations.\n");
	abort_all_soup();
	return FALSE;
}

void
network_timeout(void)
{

	if (nettime_id)
		g_source_remove(nettime_id);
	
	float timeout = gconf_client_get_float(rss_gconf, GCONF_KEY_NETWORK_TIMEOUT, NULL);
	if (!timeout)
               timeout = NETWORK_MIN_TIMEOUT;

	nettime_id = g_timeout_add (
				timeout*1000,
				(GtkFunction) timeout_soup,
                           	0);
}

void
abort_all_soup(void)
{
	//abort all session
	rf->cancel = 1;
	rf->cancel_all = 1;
	if (rf->abort_session)
	{
		g_hash_table_foreach(rf->abort_session, remove_weak, NULL);
		g_hash_table_foreach_remove(rf->abort_session, cancel_soup_sess, NULL);
//		g_hash_table_foreach(rf->abort_session, cancel_soup_sess, NULL);
		g_hash_table_destroy(rf->session);
                rf->session = g_hash_table_new(g_direct_hash, g_direct_equal);
	}
	if (rf->progress_bar)
	{
		gtk_progress_bar_set_fraction((GtkProgressBar *)rf->progress_bar, 1);
		rf->progress_bar = NULL;	//there's no need to update bar once we canceled feeds
	}
	if (rf->b_session)
	{
/*		if (SOUP_IS_MESSAGE(rf->b_msg_session))
		{
#if LIBSOUP_VERSION < 2003000
			soup_message_set_status(rf->b_msg_session, SOUP_STATUS_CANCELLED);
			soup_session_cancel_message(rf->b_session, rf->b_msg_session);
#else
			soup_session_cancel_message(rf->b_session, rf->b_msg_session, SOUP_STATUS_CANCELLED);
#endif
		}*/
		soup_session_abort(rf->b_session);
		rf->b_session = NULL;
		rf->b_msg_session = NULL;
	}
	rf->cancel_all = 0;
}

static void
readrss_dialog_cb (GtkWidget *widget, gpointer data)
{
	d(g_print("\nCancel reading feeds\n"));
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

gchar *
feed_to_xml(gchar *key)
{
	xmlNodePtr root, node, id, src, xport;
        char *tmp, buf[20];
        xmlChar *xmlbuf;
        xmlDocPtr doc;
        int n;
	gchar *ctmp;

        doc = xmlNewDoc ("1.0");

        root = xmlNewDocNode (doc, NULL, "feed", NULL);
        xmlDocSetRootElement (doc, root);

        xmlSetProp (root, "uid", g_hash_table_lookup(rf->hrname, key));
        xmlSetProp (root, "enabled", g_hash_table_lookup(rf->hre, lookup_key(key)) ? "true" : "false");
        xmlSetProp (root, "html", g_hash_table_lookup(rf->hrh, lookup_key(key)) ? "true" : "false");


        xmlNewTextChild (root, NULL, "name", key);
        xmlNewTextChild (root, NULL, "url", g_hash_table_lookup(rf->hr, lookup_key(key)));
        xmlNewTextChild (root, NULL, "type", g_hash_table_lookup(rf->hrt, lookup_key(key)));

        src = xmlNewTextChild (root, NULL, "delete", NULL);
        ctmp = g_strdup_printf("%d", g_hash_table_lookup(rf->hrdel_feed, lookup_key(key)));
        xmlSetProp (src, "option", ctmp);
	g_free(ctmp);
	ctmp = g_strdup_printf("%d", g_hash_table_lookup(rf->hrdel_days, lookup_key(key)));
        xmlSetProp (src, "days", ctmp);
	g_free(ctmp);
	ctmp = g_strdup_printf("%d", g_hash_table_lookup(rf->hrdel_messages, lookup_key(key)));
        xmlSetProp (src, "messages", ctmp);
	g_free(ctmp);
        xmlSetProp (src, "unread", 
		g_hash_table_lookup(rf->hrdel_unread, lookup_key(key)) ? "true" : "false");

        src = xmlNewTextChild (root, NULL, "ttl", NULL);
	ctmp = g_strdup_printf("%d", g_hash_table_lookup(rf->hrupdate, lookup_key(key)));
        xmlSetProp (src, "option", ctmp);
	g_free(ctmp);
	ctmp = g_strdup_printf("%d", g_hash_table_lookup(rf->hrttl, lookup_key(key)));
        xmlSetProp (src, "value", ctmp);
	g_free(ctmp);
	ctmp = g_strdup_printf("%d", g_hash_table_lookup(rf->hrttl_multiply, lookup_key(key)));
        xmlSetProp (src, "factor", ctmp);
	g_free(ctmp);
	
	xmlDocDumpMemory (doc, &xmlbuf, &n);
        xmlFreeDoc (doc);

        /* remap to glib memory */
        tmp = g_malloc (n + 1);
        memcpy (tmp, xmlbuf, n);
        tmp[n] = '\0';
        xmlFree (xmlbuf);

        return tmp;

}

void
prepare_feed(gpointer key, gpointer value, gpointer user_data)
{
        char *xmlbuf;

        xmlbuf = feed_to_xml (key);
        if (xmlbuf)
                 rss_list = g_slist_append (rss_list, xmlbuf);
}

void
save_gconf_feed(void)
{

	g_hash_table_foreach(rf->hrname, prepare_feed, NULL);
	
        gconf_client_set_list (rss_gconf,
                              "/apps/evolution/evolution-rss/feeds",
                              GCONF_VALUE_STRING, rss_list, NULL);

        while (rss_list) {
                g_free (rss_list->data);
                rss_list = g_slist_remove (rss_list, rss_list->data);
        }

        gconf_client_suggest_sync (rss_gconf, NULL);
}

static gboolean
check_if_match (gpointer key, gpointer value, gpointer user_data)
{
        char *sf_href = (char *)value;
        char *int_uri = (char *)user_data;

	d(g_print("checking hay:%s for neddle:%s\n", sf_href, int_uri));

        if (!strcmp (sf_href, int_uri))
                return TRUE; /* Quit calling the callback */

        return FALSE; /* Continue calling the callback till end of table */
}

void
rss_select_folder(gchar *folder_name)
{
	CamelStore *store = mail_component_peek_local_store(NULL);
	EMFolderTreeModel *model = mail_component_peek_tree_model(mail_component_peek());
        gchar *real_name = g_strdup_printf("%s/%s", lookup_main_folder(), folder_name);
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
//      	EMFolderView *emfv = g_object_new(em_folder_view_get_type(), NULL);
//	GtkWidget *po = (GtkWidget *)model.parent_object;
  //      em_folder_tree_set_selected ((EMFolderView *)po), uri, FALSE);
//	camel_operation_end(NULL);
	g_free(uri);
	camel_object_unref (folder);
	g_free(real_name);
}

/*void
get_selected_mail(void)
{
	MailComponent *mail_component = mail_component_peek();
	MailComponentPrivate *priv = mail_component->priv;
//	EComponentView *cv = priv->component_view;
	g_print("priv:%p", priv);
	g_print("cv:%p", cv);
	GPtrArray *uids;
	void *el = g_object_get_data((GObject *)cv, "info-label");
        EMFolderView *emfv = g_object_get_data((GObject *)el, "folderview");
	uids = message_list_get_selected(emfv->list);
	g_print("selec:%d", uids->len);
	
}*/

/*static void
enable_html_cb(GtkCellRendererToggle *cell,
               gchar *path_str,
               gpointer data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreeIter  iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  gchar *name;
  gboolean fixed;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 1, &fixed, -1);
  gtk_tree_model_get (model, &iter, 2, &name, -1);
  fixed ^= 1;
  g_hash_table_replace(rf->hrh, 
			g_strdup(lookup_key(name)), 
			GINT_TO_POINTER(fixed));
  gtk_list_store_set (GTK_LIST_STORE (model), 
			&iter, 
			1, 
			fixed, 
			-1);
  gtk_tree_path_free (path);
  save_gconf_feed();
  g_free(name);
}

static void
tree_cb (GtkWidget *widget, gpointer data)
{
	GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        gchar *name;

        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(rf->treeview));
        if (gtk_tree_selection_get_selected(selection, &model, &iter))
        {
                gtk_tree_model_get (model, &iter, 2, &name, -1);
		gtk_button_set_label(data, 
			g_hash_table_lookup(rf->hre, lookup_key(name)) ? _("Disable") : _("Enable"));
		g_free(name);
        }
}*/

static void
start_check_cb (GtkWidget *widget, gpointer data)
{
    gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
    /* Save the new setting to gconf */
    gconf_client_set_bool (rss_gconf, data, active, NULL);
}

static void
dialog_key_destroy (GtkWidget *widget, gpointer data)
{
	if (data)
		g_hash_table_remove(rf->error_hash, data);
}

static void
err_destroy (GtkWidget *widget, guint response, gpointer data)
{
	gtk_widget_destroy(widget);
	rf->errdialog = NULL;
}

static gboolean
xml_set_content (xmlNodePtr node, char **val)
{
        char *buf;
        int res;

        buf = xmlNodeGetContent(node);
        if (buf == NULL) {
                res = (*val != NULL);
                if (res) {
                        g_free(*val);
                        *val = NULL;
                }
        } else {
                res = *val == NULL || strcmp(*val, buf) != 0;
                if (res) {
                        g_free(*val);
                        *val = g_strdup(buf);
                }
                xmlFree(buf);
        }

        return res;
}

static gboolean
xml_set_prop (xmlNodePtr node, const char *name, char **val)
{
        char *buf;
        int res;

        buf = xmlGetProp (node, name);
        if (buf == NULL) {
                res = (*val != NULL);
                if (res) {
                        g_free(*val);
                        *val = NULL;
                }
        } else {
                res = *val == NULL || strcmp(*val, buf) != 0;
                if (res) {
                        g_free(*val);
                        *val = g_strdup(buf);
                }
                xmlFree(buf);
        }

        return res;
}

static gboolean
xml_set_bool (xmlNodePtr node, const char *name, gboolean *val)
{
        gboolean gbool;
        char *buf;

        if ((buf = xmlGetProp (node, name))) {
                gbool = (!strcmp (buf, "true") || !strcmp (buf, "yes"));
                xmlFree (buf);

                if (gbool != *val) {
                        *val = gbool;
                        return TRUE;
                }
        }

        return FALSE;
}

gboolean
feed_new_from_xml(char *xml)
{
	xmlNodePtr node, cur;
        xmlDocPtr doc;
        gboolean changed = FALSE;
	char *uid = NULL;
	char *name = NULL;
	char *url = NULL;
	char *type = NULL;
	gboolean enabled;
	gboolean html;
	guint del_feed=0;
	guint del_days=0;
	guint del_messages=0;
	guint del_unread=0;
	guint ttl=0;
	guint ttl_multiply=0;
	guint update=0;
	gchar *ctmp = NULL;

        if (!(doc = xmlParseDoc ((char *)xml)))
                return FALSE;

        node = doc->children;
        if (strcmp (node->name, "feed") != 0) {
                xmlFreeDoc (doc);
                return FALSE;
        }

        xml_set_prop (node, "uid", &uid);
        xml_set_bool (node, "enabled", &enabled);
        xml_set_bool (node, "html", &html);

        for (node = node->children; node; node = node->next) {
                if (!strcmp (node->name, "name")) {
			xml_set_content (node, &name);
		}
                if (!strcmp (node->name, "url")) {
			xml_set_content (node, &url);
		}
                if (!strcmp (node->name, "type")) {
			xml_set_content (node, &type);
		}
		if (!strcmp (node->name, "delete")) {
			xml_set_prop (node, "option", &ctmp);
			del_feed = atoi(ctmp);
			xml_set_prop (node, "days", &ctmp);
			del_days = atoi(ctmp);
			xml_set_prop (node, "messages", &ctmp);
			del_messages = atoi(ctmp);
			xml_set_bool (node, "unread", &del_unread);
		}
		if (!strcmp (node->name, "ttl")) {
			xml_set_prop (node, "option", &ctmp);
			update = atoi(ctmp);
			xml_set_prop (node, "value", &ctmp);
			ttl = atoi(ctmp);
			xml_set_prop (node, "factor", &ctmp);
			if (ctmp)
				ttl_multiply = atoi(ctmp);
			if (ctmp) g_free(ctmp);
		}
			
	}

	g_hash_table_insert(rf->hrname, name, uid);
	g_hash_table_insert(rf->hrname_r, g_strdup(uid), g_strdup(name));
	g_hash_table_insert(rf->hr, g_strdup(uid), url);
	g_hash_table_insert(rf->hrh, 
				g_strdup(uid), 
				GINT_TO_POINTER(html));
	g_hash_table_insert(rf->hrt, g_strdup(uid), type);
	g_hash_table_insert(rf->hre, 
				g_strdup(uid), 
				GINT_TO_POINTER(enabled));
	g_hash_table_insert(rf->hrdel_feed, 
				g_strdup(uid), 
				GINT_TO_POINTER(del_feed));
	g_hash_table_insert(rf->hrdel_days, 
				g_strdup(uid), 
				GINT_TO_POINTER(del_days));
	g_hash_table_insert(rf->hrdel_messages, 
				g_strdup(uid), 
				GINT_TO_POINTER(del_messages));
	g_hash_table_insert(rf->hrdel_unread, 
				g_strdup(uid), 
				GINT_TO_POINTER(del_unread));
	g_hash_table_insert(rf->hrupdate, 
				g_strdup(uid), 
				GINT_TO_POINTER(update));
	g_hash_table_insert(rf->hrttl, 
				g_strdup(uid), 
				GINT_TO_POINTER(ttl));
	g_hash_table_insert(rf->hrttl_multiply, 
				g_strdup(uid), 
				GINT_TO_POINTER(ttl_multiply));
}

char *
feeds_uid_from_xml (const char *xml)
{
        xmlNodePtr node;
        xmlDocPtr doc;
        char *uid = NULL;

        if (!(doc = xmlParseDoc ((char *)xml)))
                return NULL;

        node = doc->children;
        if (strcmp (node->name, "feed") != 0) {
                xmlFreeDoc (doc);
                return NULL;
        }

        xml_set_prop (node, "uid", &uid);
        xmlFreeDoc (doc);

        return uid;
}

void
load_gconf_feed(void)
{
        GSList *list, *l = NULL;
        char *uid;

        list = gconf_client_get_list (rss_gconf, "/apps/evolution/evolution-rss/feeds",
                                      GCONF_VALUE_STRING, NULL);
        for (l = list; l; l = l->next) {
                uid = feeds_uid_from_xml (l->data);
                if (!uid)
                        continue;

        	feed_new_from_xml (l->data);

                g_free (uid);
        }
}

void
migrate_old_config(gchar *feed_file)
{
	FILE *ffile;
	gchar rfeed[512];
	char **str;
	memset(rfeed, 0, 512);

	if (ffile = fopen(feed_file, "r"))
        {
		while (fgets(rfeed, 511, ffile) != NULL)
		{
                        str = g_strsplit(rfeed, "--", 0);
                        gpointer key = gen_md5(str[1]);
                        g_hash_table_insert(rf->hrname, g_strdup(str[0]), g_strdup(key));
                        g_hash_table_insert(rf->hrname_r, g_strdup(key), g_strdup(str[0]));
                        g_hash_table_insert(rf->hr, g_strdup(key), g_strstrip(str[1]));
                        if (NULL != str[4])
                        {
                                g_hash_table_insert(rf->hrh, g_strdup(key), 
                                        GINT_TO_POINTER(atoi(g_strstrip(str[4]))));
                                g_hash_table_insert(rf->hrt, g_strdup(key), g_strdup(str[3]));
                                g_hash_table_insert(rf->hre, g_strdup(key), 
                                        GINT_TO_POINTER(atoi(str[2])));
                        }
                        else
                        {
                                if (NULL != str[2])     // 0.0.1 -> 0.0.2
                                {
                                        g_hash_table_insert(rf->hrh, g_strdup(key), (gpointer)0);
                                        g_hash_table_insert(rf->hrt, g_strdup(key), g_strstrip(str[3]));
                                        g_hash_table_insert(rf->hre, g_strdup(key), 
                                                GINT_TO_POINTER(atoi(str[2])));
                                }
                                else
                                {
                                        g_hash_table_insert(rf->hrh, g_strdup(key),  (gpointer)0);
                                        g_hash_table_insert(rf->hrt, g_strdup(key), g_strdup("RSS"));
                                        g_hash_table_insert(rf->hre, g_strdup(key), 
                                        (gpointer)1);
                                }
                        }
                        g_free(key);
		}
		fclose(ffile);
		save_gconf_feed();
		unlink(feed_file);
        }
}

guint
read_feeds(rssfeed *rf)
{
	guint res = 0;
	//contruct feeds
	gchar *feed_dir = rss_component_peek_base_directory(mail_component_peek());
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
	    g_mkdir_with_parents (feed_dir, 0755);
	gchar *feed_file = g_strdup_printf("%s/evolution-feeds", feed_dir);
	g_free(feed_dir);
	rf->hrname = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	rf->hrname_r = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	rf->hr = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	rf->hre = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	rf->hrt = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	rf->hrh = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	rf->hruser = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	rf->hrpass = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	rf->hrdel_feed = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	rf->hrdel_days = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	rf->hrdel_messages = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	rf->hrdel_unread = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	rf->hrupdate = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	rf->hrttl = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	rf->hrttl_multiply = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	if (g_file_test(feed_file, G_FILE_TEST_EXISTS))
		migrate_old_config(feed_file);
	else
		load_gconf_feed();

	res = 1;
out:	g_free(feed_file);
	return res;
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
	guint engine = fallback_engine();
#ifdef	HAVE_GECKO
	if (engine == 2)
		gtk_moz_embed_go_back(GTK_MOZ_EMBED(rf->mozembed));
#endif
#if HAVE_WEBKIT
	if (engine == 1)
		webkit_web_view_stop_loading(WEBKIT_WEB_VIEW(rf->mozembed));
#endif
}

static void
forward_cb (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	guint engine = fallback_engine();
#ifdef	HAVE_GECKO
	if (engine == 2)
		gtk_moz_embed_go_forward(GTK_MOZ_EMBED(rf->mozembed));
#endif
#if HAVE_WEBKIT
	if (engine == 1)
		webkit_web_view_stop_loading(WEBKIT_WEB_VIEW(rf->mozembed));
#endif
}

static void
stop_cb (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	guint engine = fallback_engine();
#ifdef	HAVE_GECKO
	if (engine == 2)
		gtk_moz_embed_stop_load(GTK_MOZ_EMBED(rf->mozembed));
#endif
#if HAVE_WEBKIT
	if (engine == 1)
		webkit_web_view_stop_loading(WEBKIT_WEB_VIEW(rf->mozembed));
#endif
}

reload_cb (GtkWidget *button, gpointer data)
{
	guint engine = gconf_client_get_int(rss_gconf, GCONF_KEY_HTML_RENDER, NULL);
	switch (engine)
	{
		case 2:
#ifdef	HAVE_GECKO
	gtk_moz_embed_stop_load(GTK_MOZ_EMBED(rf->mozembed));
       	gtk_moz_embed_load_url (GTK_MOZ_EMBED(rf->mozembed), data);
#endif
		break;
		case 1:
#ifdef	HAVE_WEBKIT
	webkit_web_view_stop_loading(WEBKIT_WEB_VIEW(rf->mozembed));
     	webkit_web_view_open(WEBKIT_WEB_VIEW(rf->mozembed), data);
#endif
		break;
	}
}


static void
mycall (GtkWidget *widget, GtkAllocation *event, gpointer data)
{
//	GtkAdjustment *a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(widget));
//	g_print("page size:%d\n", a->page_size);
	int width;
        GtkRequisition req;
  	struct _org_gnome_rss_controls_pobject *po = data;

	guint k = rf->headers_mode ? 240 : 106;
	if (GTK_IS_WIDGET(widget))
	{
        	width = widget->allocation.width - 16 - 2;// - 16;
        	int height = widget->allocation.height - 16 - k;
		d(g_print("resize webkit :width:%d, height: %d\n", width, height));
//	EMFormat *myf = (EMFormat *)efh;
//	GtkRequisition req;
//	gtk_widget_size_request(data, &req);
//	GtkWidget *my = data;
//	g_print("w:%d,h:%d\n", req.width, req.height);
//	g_print("w2:%d,h2:%d\n", my->allocation.width, my->allocation.height);
//	int wheight = height - (req.height - height) - 20;
//        height = req.height - 200;// - 16 - 194;
//        
		if (po->mozembedwindow && rf->mozembed)
			if(GTK_IS_WIDGET(po->mozembedwindow) && height > 0)
			{
				gtk_moz_embed_open_stream(GTK_MOZ_EMBED(rf->mozembed),
		    		po->website, "text/html");
		//browserwrite("test", 4);
				if (!browser_fetching) {
					gint fill=0;
					browser_fetching=1;
					fetch_unblocking(
						po->website,
						browsercb,
						1,
						(gpointer)finish_website,
						1,	// we need to dupe key here
						1,
						NULL);
				}
/*				gchar *str = content->str;
				gint len = strlen(content->str);
				while (len > 0) {
					if (len > 4096) {
						gtk_moz_embed_append_data(GTK_MOZ_EMBED(rf->mozembed),
							str, 4096);
					str+=4096;
					}
					else
						gtk_moz_embed_append_data(GTK_MOZ_EMBED(rf->mozembed),
			    				str, len);
				len-=4096;
				}
				gtk_moz_embed_close_stream(GTK_MOZ_EMBED(rf->mozembed));*/
				gtk_widget_set_size_request((GtkWidget *)po->mozembedwindow, width, height);
// apparently resizing gtkmozembed widget won't redraw if using xulrunner
// there is no point in reload for the rest
/*#if defined(HAVE_XULRUNNER)
// || defined(HAVE_GECKO_1_9)
if (2 == gconf_client_get_int(rss_gconf, GCONF_KEY_HTML_RENDER, NULL))
	gtk_moz_embed_reload((GtkMozEmbed *)rf->mozembed, GTK_MOZ_EMBED_FLAG_RELOADNORMAL);
#endif*/
			}
	}
}

#ifdef HAVE_GECKO
void
rss_mozilla_init(void)
{
	GError *err = NULL;
/*#ifdef GECKO_HOME
	g_setenv("MOZILLA_FIVE_HOME", GECKO_HOME, 1);
#endif
	g_unsetenv("MOZILLA_FIVE_HOME");*/


	gecko_init();
}
#endif

#ifdef HAVE_GECKO
void
render_set_preferences(void)
{
	gecko_prefs_set_bool("javascript.enabled", 
		gconf_client_get_bool(rss_gconf, GCONF_KEY_HTML_JS, NULL));
	gecko_prefs_set_bool("security.enable_java", 
		gconf_client_get_bool(rss_gconf, GCONF_KEY_HTML_JAVA, NULL));
	gecko_prefs_set_bool("plugin.scan.plid.all", FALSE);
	gecko_prefs_set_bool("plugin.default_plugin_disabled", TRUE); 
	gchar *agstr = g_strdup_printf("Evolution/%s; Evolution-RSS/%s",
                        EVOLUTION_VERSION_STRING, VERSION);
	gecko_prefs_set_string("general.useragent.extra.firefox", agstr); 
	g_free(agstr);
}
#endif

#ifdef HAVE_RENDERKIT
static gboolean
org_gnome_rss_browser (EMFormatHTML *efh, void *eb, EMFormatHTMLPObject *pobject)
{
	struct _org_gnome_rss_controls_pobject *po = (struct _org_gnome_rss_controls_pobject *) pobject;
	int width, height;
        GtkRequisition req;
	GtkWidget *moz;
	GString *content;

//        gtk_widget_size_request (efhd->priv->attachment_bar, &req);
	guint engine = fallback_engine();
	moz = gtk_scrolled_window_new(NULL,NULL);
//	moz = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(moz),
                                       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	

#ifdef HAVE_WEBKIT
	if (engine == 1)
	{
		rf->mozembed = (GtkWidget *)webkit_web_view_new();
		gtk_container_add(GTK_CONTAINER(moz), GTK_WIDGET(rf->mozembed));
		//gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(moz), GTK_WIDGET(rf->mozembed));
		//gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(moz), GTK_SHADOW_ETCHED_OUT);
	}
#endif

#ifdef HAVE_GECKO
	if (engine == 2)
	{
		rf->mozembed = gtk_moz_embed_new();
		render_set_preferences();

		/* FIXME add all those profile shits */
		gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(moz), GTK_WIDGET(rf->mozembed));
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(moz), GTK_SHADOW_ETCHED_OUT);
	}
#endif

	po->container = moz;

#ifdef HAVE_WEBKIT
	if (engine == 1)
	{
		d(g_print("Render engine Webkit\n"));
		if (rf->online)
        		webkit_web_view_open(WEBKIT_WEB_VIEW(rf->mozembed), po->website);
		else
        		webkit_web_view_open(WEBKIT_WEB_VIEW(rf->mozembed), "about:blank");
	}
#endif

#ifdef HAVE_GECKO
	if (engine == 2)
	{
		d(g_print("Render engine Gecko\n"));
		if (rf->online)
		{
			//gtk_moz_embed_stop_load(GTK_MOZ_EMBED(rf->mozembed));
 	      		//gtk_moz_embed_load_url (GTK_MOZ_EMBED(rf->mozembed), po->website);
		}
		else	
		{
			gtk_moz_embed_stop_load(GTK_MOZ_EMBED(rf->mozembed));
        		gtk_moz_embed_load_url (GTK_MOZ_EMBED(rf->mozembed), "about:blank");
		}
	}
#endif


//	gtk_container_set_resize_mode(w, GTK_RESIZE_PARENT);
//	gtk_scrolled_window_set_policy(w, GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	gtk_widget_show_all(moz);
        gtk_container_add ((GtkContainer *) eb, moz);
        gtk_container_check_resize ((GtkContainer *) eb);
//	gtk_widget_set_size_request((GtkWidget *)rf->mozembed, 330, 330);
//        gtk_container_add ((GtkContainer *) eb, rf->mozembed);
	EMFormat *myf = (EMFormat *)efh;
	rf->headers_mode = myf->mode;
	po->mozembedwindow =  moz;
	po->shandler = g_signal_connect(efh->html,
		"size_allocate",
		G_CALLBACK(mycall),
		po);
	return TRUE;
}
#endif

static gboolean
org_gnome_rss_rfrcomm (EMFormatHTML *efh, void *eb, EMFormatHTMLPObject *pobject)
{
        struct _org_gnome_rss_controls_pobject *po = (struct _org_gnome_rss_controls_pobject *) pobject;
	GtkWidget *hbox = gtk_hbox_new (FALSE, 0);

	gchar *mem = g_strdup_printf("%s(%d):",  _("Comments"), po->counter);
	GtkWidget *label = gtk_link_button_new_with_label(po->website, mem);
	gtk_widget_show (label);
	g_free(mem);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	GtkWidget *button = gtk_button_new_with_label(_("Refresh"));
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
	struct _org_gnome_rss_controls_pobject *po = (struct _org_gnome_rss_controls_pobject *) pobject;
	GtkWidget *vbox = gtk_vbox_new (TRUE, 1);
	GtkWidget *hbox2 = gtk_hbox_new (FALSE, 0);

	GtkWidget *label3 = gtk_label_new ("");
	gchar *mem = g_strdup_printf(" <b>%s: </b>", _("Feed view"));
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label3), mem);
	gtk_widget_show (label3);
	gtk_box_pack_start (GTK_BOX (hbox2), label3, TRUE, TRUE, 0);

	GtkWidget *button = gtk_button_new_with_label(
				rf->cur_format ? _("Show Summary") : _("Show Full Text"));

	gtk_button_set_image (
                GTK_BUTTON (button),
		gtk_image_new_from_icon_name (
                        rf->cur_format ? "text-x-generic" : "text-html",
			GTK_ICON_SIZE_BUTTON));

	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_HALF);
	g_signal_connect (button, "clicked", G_CALLBACK(summary_cb), efh);
	gtk_box_pack_start (GTK_BOX (hbox2), button, TRUE, TRUE, 0);
        gtk_widget_show_all (button);
	if (rf->cur_format)
	{
        	GtkWidget *button4 = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
		g_signal_connect (button4, "clicked", G_CALLBACK(back_cb), efh);
//		gtk_widget_set_size_request(button4, 100, 10);
		gtk_button_set_relief(GTK_BUTTON(button4), GTK_RELIEF_HALF);
		gtk_widget_set_sensitive (button4, rf->online);
        	gtk_widget_show (button4);
		gtk_box_pack_start (GTK_BOX (hbox2), button4, TRUE, TRUE, 0);
        	GtkWidget *button5 = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
		g_signal_connect (button5, "clicked", G_CALLBACK(forward_cb), efh);
//		gtk_widget_set_size_request(button5, 100, 10);
		gtk_button_set_relief(GTK_BUTTON(button5), GTK_RELIEF_HALF);
		gtk_widget_set_sensitive (button5, rf->online);
        	gtk_widget_show (button5);
		gtk_box_pack_start (GTK_BOX (hbox2), button5, TRUE, TRUE, 0);
        	GtkWidget *button2 = gtk_button_new_from_stock (GTK_STOCK_STOP);
		g_signal_connect (button2, "clicked", G_CALLBACK(stop_cb), efh);
//		gtk_widget_set_size_request(button2, 100, 10);
		gtk_button_set_relief(GTK_BUTTON(button2), GTK_RELIEF_HALF);
		gtk_widget_set_sensitive (button2, rf->online);
        	gtk_widget_show (button2);
		gtk_box_pack_start (GTK_BOX (hbox2), button2, TRUE, TRUE, 0);
        	GtkWidget *button3 = gtk_button_new_from_stock (GTK_STOCK_REFRESH);
		g_signal_connect (button3, "clicked", G_CALLBACK(reload_cb), po->website);
//		gtk_widget_set_size_request(button3, 100, -1);
		gtk_button_set_relief(GTK_BUTTON(button3), GTK_RELIEF_HALF);
		gtk_widget_set_sensitive (button3, rf->online);
        	gtk_widget_show (button3);
		gtk_box_pack_start (GTK_BOX (hbox2), button3, TRUE, TRUE, 0);
//	gtk_widget_show (hbox2);
	}
	gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0);
	gtk_widget_show_all (vbox);

      	int width = vbox->allocation.width;
       	int height = vbox->allocation.height;

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
free_rss_controls(EMFormatHTMLPObject *o)
{
	struct _org_gnome_rss_controls_pobject *po = (struct _org_gnome_rss_controls_pobject *) o;
	if (po->mem)
		g_free(po->mem);
	if (po->website)
		g_free(po->website);
	gtk_widget_destroy(po->html);
}

void
pfree(EMFormatHTMLPObject *o)
{
	struct _org_gnome_rss_controls_pobject *po = (struct _org_gnome_rss_controls_pobject *) o;
	guint engine = gconf_client_get_int(rss_gconf, GCONF_KEY_HTML_RENDER, NULL);
#ifdef HAVE_GECKO
	if (engine == 2)
	{
		gtk_moz_embed_stop_load(GTK_MOZ_EMBED(rf->mozembed));
//		gtk_moz_embed_pop_startup();
	}
#endif
	g_signal_handler_disconnect(po->format->html, po->shandler);
/*	if (rf->mozembed)
	{
		gtk_widget_destroy(rf->mozembed);
		rf->mozembed = NULL;
	}*/
	gtk_widget_destroy(po->container);
	g_free(po->website);
	browser_fetching = 0;
}

EMFormat *fom;
CamelStream *som;

void org_gnome_cooly_format_rss(void *ep, EMFormatHookTarget *t);

void org_gnome_cooly_format_rss(void *ep, EMFormatHookTarget *t)	//camelmimepart
{
        GError *err = NULL;
        GString *content;
	xmlChar *buff = NULL;
	int size = 0;
	CamelContentType *type;
	gchar *feedid = NULL;
	gchar *comments = NULL;
	gchar *category = NULL;
	GdkPixbuf *pixbuf = NULL;
	CamelDataWrapper *dw = camel_data_wrapper_new();
	CamelMimePart *part = camel_mime_part_new();
	CamelStream *fstream = camel_stream_mem_new();
        d(g_print("Formatting...\n"));

	CamelMimePart *message = CAMEL_IS_MIME_MESSAGE(t->part) ? 
			t->part : 
			(CamelMimePart *)t->format->message;

	EMFormatHTML *emfh = (EMFormatHTML *)t->format;
	/* force loading of images even if mail images disabled */
	emfh->load_http_now = TRUE;
	/* assuming 0xffffff will ruin dark themes */
	frame_colour = emfh->frame_colour;// ? emfh->frame_colour: 0xffffff;
	content_colour = emfh->content_colour;// ? emfh->content_colour: 0xffffff;
	text_colour = emfh->text_colour;// ? emfh->text_colour: 0xffffff;

	type = camel_mime_part_get_content_type(message);
	const char *website = camel_medium_get_header (CAMEL_MEDIUM (message), "Website");
	if (!website)
		goto fmerror;
	gchar *addr = (gchar *)camel_header_location_decode(website);
	feedid  = (gchar *)camel_medium_get_header (CAMEL_MEDIUM(message), "RSS-ID");
	comments  = (gchar *)camel_medium_get_header (CAMEL_MEDIUM(message), "X-Evolution-rss-comments");
	category  = (gchar *)camel_medium_get_header (CAMEL_MEDIUM(message), "X-Evolution-rss-category");
	gchar *subject = camel_header_decode_string(camel_medium_get_header (CAMEL_MEDIUM (message),
				 "Subject"), NULL);
	gchar *f = camel_header_decode_string(camel_medium_get_header (CAMEL_MEDIUM (message),
				 "From"), NULL);
	
	gpointer is_html = NULL;
	if (feedid)
		is_html =  g_hash_table_lookup(rf->hrh, g_strstrip(feedid));
	if (comments)
		comments = g_strstrip(comments);
	
	if (!rf->chg_format)
		rf->cur_format = GPOINTER_TO_INT(is_html);
	
	if (rf->chg_format)
		rf->chg_format = 0;

	struct _org_gnome_rss_controls_pobject *pobj;
        char *classid = g_strdup_printf ("org-gnome-rss-controls-%d",
			org_gnome_rss_controls_counter_id);
	org_gnome_rss_controls_counter_id++;
	pobj = (struct _org_gnome_rss_controls_pobject *) em_format_html_add_pobject ((EMFormatHTML *) t->format, sizeof(*pobj), classid, message, (EMFormatHTMLPObjectFunc)org_gnome_rss_controls);
	pobj->is_html = GPOINTER_TO_INT(is_html);
	pobj->website = g_strstrip(g_strdup((gchar *)website));
	pobj->stream = t->stream;
	pobj->object.free = free_rss_controls;
        camel_stream_printf (t->stream, "<object classid=%s></object>\n", classid);


	if (rf->cur_format || (feedid && is_html && rf->cur_format))
	{
		guint engine = fallback_engine();
#ifdef HAVE_RENDERKIT
		if (engine && engine != 10)
		{ 
        		char *classid = g_strdup_printf ("org-gnome-rss-controls-%d",
				org_gnome_rss_controls_counter_id);
			org_gnome_rss_controls_counter_id++;
			pobj = (struct _org_gnome_rss_controls_pobject *) 
					em_format_html_add_pobject ((EMFormatHTML *) t->format, 
										sizeof(*pobj), 
										classid, 
										message, 
										(EMFormatHTMLPObjectFunc)org_gnome_rss_browser);
			pobj->website = g_strstrip(g_strdup((gchar *)website));
			pobj->is_html = GPOINTER_TO_INT(is_html);
			pobj->format = (EMFormatHTML *)t->format;
			pobj->object.free = pfree;
			camel_stream_printf (t->stream,
				"<div style=\"border: solid #%06x 1px; background-color: #%06x; color: #%06x;\">\n",
				frame_colour & 0xffffff, content_colour & 0xffffff, text_colour & 0xffffff);
			camel_stream_printf(t->stream,
		 		"<table border=0 width=\"100%%\" cellpadding=1 cellspacing=1><tr><td>");
        		camel_stream_printf (t->stream, 
				"<object classid=%s></object></td></tr></table></div>\n", classid);
			goto out;
		}
#endif
		//replace with unblocking
		content = fetch_blocking(addr, NULL, NULL, textcb, NULL, &err);
		if (err)
        	{
			//we do not need to setup a pop error menu since we're in 
			//formatting process. But instead display mail body an error
			//such proxy error or transport error
			camel_stream_printf (t->stream,
				"<div style=\"border: solid #%06x 1px; background-color: #%06x; color: #%06x;\">\n",
				frame_colour & 0xffffff, content_colour & 0xffffff, text_colour & 0xffffff);
			camel_stream_printf(t->stream, 
        			"<div style=\"border: solid 0px; padding: 4px;\">\n");
     			camel_stream_printf (t->stream, "<h3>Error!</h3>");
     			camel_stream_printf (t->stream, "%s", err->message);
    			camel_stream_printf (t->stream, "</div>");
                	goto out;
        	}

		gchar *tmp = decode_utf8_entities(content->str);
		xmlDoc *src = (xmlDoc *)parse_html(addr, tmp, strlen(tmp));

		if (src)
		{
			htmlDocDumpMemory(src, &buff, &size);
			d(g_print("htmlDocDumpMemory:%s\n", buff));
			xmlFree(src);
		}
		else
			goto out;

		camel_stream_printf (fstream,
			"<div style=\"border: solid #%06x 1px; background-color: #%06x; color: #%06x;\">\n",
			frame_colour & 0xffffff, content_colour & 0xffffff, text_colour & 0xffffff);
                camel_stream_printf (fstream,
                        "<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
                        "<b><font size=+1><a href=%s>%s</a></font></b></div>",
			content_colour & 0xEDECEB & 0xffffff, text_colour & 0xffffff,
                        website, subject);
                if (category)
                        camel_stream_printf(fstream,
                                "<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
                                "<b><font size=-1>Posted under: %s</font></b></div>",
                                content_colour & 0xEDECEB & 0xffffff, text_colour & 0xffffff,
                                category);
                camel_stream_printf (fstream, "<div style=\"border: solid #%06x 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
                                "%s</div>",
                        	frame_colour & 0xffffff, content_colour & 0xffffff, text_colour & 0xffffff,
                                buff);

		g_free(subject);
		g_string_free(content, 1);
	}
	else
	{
		d(g_print("normal html rendering\n"));
		GByteArray *buffer;
		CamelStreamMem *stream = (CamelStreamMem *)camel_stream_mem_new();
		buffer = g_byte_array_new ();
        	camel_stream_mem_set_byte_array (stream, buffer);

		CamelDataWrapper *content = camel_medium_get_content_object(CAMEL_MEDIUM(t->part));
     		camel_data_wrapper_write_to_stream(content, (CamelStream *)stream);
		g_byte_array_append (buffer, "", 1);
//#ifdef EVOLUTION_2_12	//aparently this ("?" char parsing) is fixed in 2.12
//		//then again this does not work in evo > 2.12 perhaps is gtkhtml related 
//		buff = buffer->data;
//#else
		gchar *tmp;
	 	if (camel_content_type_is(type, "text", "evolution-rss-feed"))	//old evolution-rss content type
		{
			tmp = decode_utf8_entities(buffer->data);
		}
		else
			tmp = g_strdup(buffer->data);

		buff=tmp;
		g_byte_array_free (buffer, 1);
	//	char *buff = decode_html_entities(buffer2);
///		buff=tmp;


		gchar *feed_dir = rss_component_peek_base_directory(mail_component_peek());
                gchar *feed_file = g_strdup_printf("%s/%s.img", feed_dir, feedid);

		camel_stream_printf (fstream,
                        "<div style=\"border: solid #%06x 1px; background-color: #%06x; padding: 2px; color: #%06x;\">",
                        frame_colour & 0xffffff, content_colour & 0xEDECEB & 0xffffff, text_colour & 0xffffff);
        	if (g_file_test(feed_file, G_FILE_TEST_EXISTS))
			if (pixbuf = gdk_pixbuf_new_from_file(feed_file, NULL)) {
                		camel_stream_printf (fstream,
                        	"<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
                        	"<img height=16 src=%s>"
                        	"<b><font size=+1><a href=%s>%s</a></font></b></div>",
				content_colour & 0xEDECEB & 0xffffff, text_colour & 0xffffff,
                        	feed_file, website, subject);
				g_object_unref(pixbuf);
				goto render_body;
			}
		gchar *iconfile = g_build_filename (EVOLUTION_ICONDIR,
                                            "rss-16.png",
                                                NULL);
      		camel_stream_printf (fstream,
                       	"<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
                        "<img height=16 src=%s>"
                       	"<b><font size=+1><a href=%s>%s</a></font></b></div>",
			content_colour & 0xEDECEB & 0xffffff, text_colour & 0xffffff,
                       	iconfile, website, subject);
		g_free(iconfile);
render_body:    if (category)
                        camel_stream_printf(fstream,
                                "<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
                                "<b><font size=-1>Posted under: %s</font></b></div>",
                                content_colour & 0xEDECEB & 0xffffff, text_colour & 0xffffff,
                                category);
                camel_stream_printf (fstream, "<div style=\"border: solid #%06x 0px; background-color: #%06x; padding: 10px; color: #%06x;\">"
                                "%s</div>",
                        	frame_colour & 0xffffff, content_colour & 0xffffff, text_colour & 0xffffff,
                                buff);

		if (comments) {
			if (commstream) {
			camel_stream_printf (fstream, 
                        	"<div style=\"border: solid #%06x 0px; background-color: #%06x; padding: 2px; color: #%06x;\">",
                        	frame_colour & 0xffffff, content_colour & 0xEDECEB & 0xffffff, text_colour & 0xffffff);
				gchar *result = print_comments(comments, commstream);
				char *rfrclsid = g_strdup_printf ("org-gnome-rss-controls-%d",
					org_gnome_rss_controls_counter_id);
				org_gnome_rss_controls_counter_id++;
				pobj = (struct _org_gnome_rss_controls_pobject *) em_format_html_add_pobject ((EMFormatHTML *) t->format, sizeof(*pobj), rfrclsid, message, (EMFormatHTMLPObjectFunc)org_gnome_rss_rfrcomm);
				pobj->counter = commcnt;
				pobj->website = comments;
//				pobj->object.free = free_rss_controls;
				camel_stream_printf(fstream, 
                       			"<object height=25 classid=%s></object>", rfrclsid);
				if (result && strlen(result))
					camel_stream_printf(fstream, 
					"<div style=\"border: solid #%06x 0px; background-color: #%06x; padding: 10px; color: #%06x;\">%s",
						frame_colour & 0xffffff, content_colour & 0xffffff, text_colour & 0xffffff, result);
				commstream = NULL;
			}
			else {
				fetch_comments(comments, (CamelStream *)t->format);
			}
			camel_stream_printf (fstream, "</div>");
		}	
                camel_stream_printf (fstream, "</div>");
	}

	//this is required for proper charset rendering when html
      	camel_data_wrapper_construct_from_stream(dw, fstream);
      	camel_medium_set_content_object((CamelMedium *)part, dw);
	em_format_format_text((EMFormat *)t->format, (CamelStream *)t->stream, (CamelDataWrapper *)part);
	camel_object_unref(dw);
	camel_object_unref(part);
	camel_object_unref(fstream);
	g_free(buff);

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
    	camel_stream_printf (t->stream, "</div></div>");
	return;
}

void org_gnome_cooly_folder_refresh(void *ep, EMEventTargetFolder *t)
{
	g_print("refrish %s\n", t->uri);
}

#if (EVOLUTION_VERSION >= 22306)
void org_gnome_cooly_folder_icon(void *ep, EMEventTargetCustomIcon *t)
{
	static gboolean initialised = FALSE;
	GdkPixbuf *icon, *pixbuf;

	gchar *main_folder = get_main_folder();
	if (t->folder_name == NULL 
	  || g_ascii_strncasecmp(t->folder_name, main_folder, strlen(main_folder)))
		goto out;
	if (!g_ascii_strcasecmp(t->folder_name, main_folder))
		goto normal;
	gchar *rss_folder = extract_main_folder((gchar *)t->folder_name);
	if (!rss_folder)
		goto out;
	if (!icons)
		icons = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	gchar *ofolder = g_hash_table_lookup(rf->feed_folders, rss_folder);
	gchar *key = g_hash_table_lookup(rf->hrname,
				ofolder ? ofolder : rss_folder);
	if (!key)
		goto normal;
	if (!(icon = g_hash_table_lookup(icons, key))) {
  		if (gconf_client_get_bool (rss_gconf, GCONF_KEY_FEED_ICON, NULL)) {
			gchar *feed_dir = rss_component_peek_base_directory(mail_component_peek());
        		gchar *feed_file = g_strdup_printf("%s/%s.img", feed_dir, key);
        		if (g_file_test(feed_file, G_FILE_TEST_EXISTS)) {
					// unfortunately e_icon_factory_get_icon return broken image in case of error
					// we use gdk_pixbuf_new_from_file to test the validity of the image file
					pixbuf = gdk_pixbuf_new_from_file(feed_file, NULL);
					if (pixbuf) {
						icon = e_icon_factory_get_icon (feed_file, E_ICON_SIZE_MENU);
						g_hash_table_insert(icons, g_strdup(key), icon);
						g_object_set (t->renderer, "pixbuf", icon, "visible", 1, NULL);
						g_object_unref(pixbuf);
						goto out;
					}
			}
		}
	} else {
		g_object_set (t->renderer, "pixbuf", icon, "visible", 1, NULL);
		goto out;
	}

normal:	if (!initialised) //move this to startup
	{
		gchar *iconfile = g_build_filename (EVOLUTION_ICONDIR,
	                                    "rss-16.png",
						NULL);
		folder_icon = e_icon_factory_get_icon (iconfile, E_ICON_SIZE_MENU);
		g_free(iconfile);
		initialised = TRUE;
	}
	g_object_set (t->renderer, "pixbuf", folder_icon, "visible", 1, NULL);
out:	g_free(main_folder);
	return;
}
#endif

#ifdef EVOLUTION_2_12
void org_gnome_cooly_article_show(void *ep, EMEventTargetMessage *t);
#else
void org_gnome_cooly_article_show(void *ep, void *t);
#endif

#ifdef EVOLUTION_2_12
void org_gnome_cooly_article_show(void *ep, EMEventTargetMessage *t)
{
	if (rf && (!inhibit_read || !delete_op))
		rf->current_uid = g_strdup(t->uid);
}
#else
void org_gnome_cooly_article_show(void *ep, void *t)
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
	gchar *stmp;
	while (check_chn_name(chn_name))
	{
		GString *result = g_string_new (NULL);
		gchar *tmp = chn_name;
		if (c = strrchr(tmp, '#'))
		{
			if (isdigit(*(c+1)))
			{
				stmp = g_strndup(tmp, c - tmp);
				while (isdigit(*(c+1)))
				{
					g_string_append_c(result, *(c+1));
					c++;
				}
				i = atoi(result->str);
				chn_name = g_strdup_printf("%s#%d", stmp, i+1);
				g_free(stmp);
			}
			else
				chn_name = g_strdup_printf("%s #%d", tmp, i+1);
		}
		else
			chn_name = g_strdup_printf("%s #%d", tmp, i+1);
		memset(result->str, 0, result->len);
		g_string_free (result, TRUE);
		g_free(tmp);
	}
	return chn_name;
}

gchar *
search_rss(char *buffer, int len)
{
	gchar *app;
	xmlNode *doc = (xmlNode *)parse_html_sux (buffer, len);
	while (doc) {
		doc = html_find(doc, "link");
		app = xmlGetProp(doc, "type");
		if (!g_ascii_strcasecmp(app, "application/atom+xml")
		|| !g_ascii_strcasecmp(app, "application/xml")
		|| !g_ascii_strcasecmp(app, "application/rss+xml")) {
			return xmlGetProp(doc, "href");
		}
		xmlFree(app);
	}
	return NULL;
}

#ifdef _WIN32
char *strcasestr(const char *a, const char *b)
{
       char *a2=g_ascii_strdown(a,-1), *b2=g_ascii_strdown(b,-1), *r=strstr(a2,b2);
       if(r)
               r=a+(r-a2);
       g_free(a2);
       g_free(b2);
       return r;
}
#endif

gboolean
setup_feed(add_feed *feed)
{
	CamelException ex;
	guint ret = 0;
	guint ttl;
	guint ttl_multiply = 0;
        RDF *r = NULL;
        GString *post;
        GError *err = NULL;
        GString *content = NULL;
        GtkWidget *ed;
	gchar *chn_name = NULL;

	check_folders();

        r = g_new0 (RDF, 1);
        r->shown = TRUE;

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
						 NULL,
						 g_free);
	if (rf->hrpass == NULL)	
	    	rf->hrpass = g_hash_table_new_full(g_str_hash,
						 g_str_equal,
						 NULL,
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
	    	rf->hrdel_unread = g_hash_table_new_full(g_str_hash,
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

	rf->pending = TRUE;

	if (!feed->validate)
		goto add;
		
top:	d(g_print("adding feed->feed_url:%s\n", feed->feed_url));
        content = fetch_blocking(feed->feed_url, NULL, post, textcb, rf, &err);
        if (err)
	{
		g_print("setup_feed() -> err:%s\n", err->message);
		rss_error(NULL, feed->feed_name ? feed->feed_name: _("Unamed feed"), _("Error while fetching feed."), err->message);
		goto out;
        }
        xmlDocPtr doc = NULL;
        xmlNodePtr root = NULL;
        xmlSubstituteEntitiesDefaultValue = 0;
        doc = xml_parse_sux (content->str, content->len);
	d(g_print("content:\n%s\n", content->str));
	root = xmlDocGetRootElement(doc);

	if ((doc != NULL && root != NULL)
		&& (strcasestr(root->name, "rss")
		|| strcasestr(root->name, "rdf")
		|| strcasestr(root->name, "feed"))) {
        	r->cache = doc;
		r->uri = feed->feed_url;
		r->progress = feed->progress;

        	chn_name = display_doc (r);
add:
		//feed name can only come from an import so we rather prefer
		//resulted channel name instead of supplied one

		if (feed->feed_name && !chn_name)
	                chn_name = g_strdup(feed->feed_name);
                if (chn_name == NULL)
                        chn_name = g_strdup (DEFAULT_NO_CHANNEL);
                //FIXME g_free
		gchar *tmp = sanitize_folder(chn_name);
		g_free(chn_name);
		chn_name = tmp;
               	chn_name = generate_safe_chn_name(chn_name);
		
		gpointer crc_feed = gen_md5(feed->feed_url);
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
		g_hash_table_insert(rf->hrdel_unread,
			g_strdup(crc_feed),
			GINT_TO_POINTER(feed->del_unread));
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

		gchar *ver = NULL;
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
		g_free(chn_name);
		if (r->cache)
                	xmlFreeDoc(r->cache);
        	if (r->type)
                	g_free(r->type);
		if (r)
			g_free(r);
		if (content)
			g_string_free(content, 1);

		rf->setup = 1;
		ret = 1;
		goto out;
	}
	//search for a feed entry
	gchar *rssurl = search_rss(content->str, content->len);
	if (rssurl) {
		feed->feed_url = rssurl;
		goto top;
	}

 	rss_error(NULL, NULL, _("Error while fetching feed."), _("Invalid Feed"));
	ret = 0;

out:	rf->pending = FALSE;
	return ret;
}

void
update_sr_message(void)
{
	if (flabel && farticle)
	{
		gchar *fmsg = g_strdup_printf(_("Getting message %d of %d"), farticle, ftotal);
		gtk_label_set_text (GTK_LABEL (flabel), fmsg);
		g_free(fmsg);
	}
}

void
update_ttl(gpointer key, guint value)
{
	if (2 != GPOINTER_TO_INT(g_hash_table_lookup(rf->hrupdate, key)))
		g_hash_table_replace(rf->hrttl, g_strdup(key), GINT_TO_POINTER(value));
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

void
gio_finish_feed (GObject *object, GAsyncResult *res, gpointer user_data)
{
	gsize file_size;
        char *file_contents;
        gboolean result;

	rfMessage *rfmsg = g_new0(rfMessage, 1);

	result = g_file_load_contents_finish (G_FILE (object),
                                              res,
                                              &file_contents, &file_size,
                                              NULL, NULL);
	rfmsg->status_code = SOUP_STATUS_OK;
	rfmsg->body = file_contents;
	rfmsg->length = file_size;
	generic_finish_feed(rfmsg, user_data);
	if (result) {
                g_free (file_contents);
        }
	g_free(rfmsg);
}

void
generic_finish_feed(rfMessage *msg, gpointer user_data)
{
	GError *err = NULL;
	gchar *chn_name = NULL;
	//FIXME user_data might be out of bounds here
	gchar *key =  lookup_key(user_data);
	gboolean deleted = 0;
	//feed might get deleted while fetching
	//so we need to test for the presence of key
	if (!key)
		deleted = 1;

	MailComponent *mc = mail_component_peek ();
        if (mc->priv->quit_state != -1)
		rf->cancel_all=1;

	d(g_print("taskbar_op_finish() queue:%d\n", rf->feed_queue));

	if (rf->feed_queue)
	{
		rf->feed_queue--;
		gchar *tmsg = g_strdup_printf(_("Fetching Feeds (%d enabled)"), g_hash_table_size(rf->hrname));
		taskbar_op_set_progress("main", tmsg, rf->feed_queue ? ((gfloat)((100-(rf->feed_queue*100/g_hash_table_size(rf->hrname))))/100): 1);
		g_free(tmsg);
	}

	if (rf->feed_queue == 0)
	{
		d(g_print("taskbar_op_finish()\n"));
		taskbar_op_finish("main");
		farticle=0;
		ftotal=0;
#ifndef EVOLUTION_2_12
		if(rf->progress_dialog)
        	{
        	     	gtk_widget_destroy(rf->progress_dialog);
       			rf->progress_dialog = NULL;
			rf->progress_bar = NULL;
        	}
#else
		if(rf->label && rf->info)
		{
                        gtk_label_set_markup (GTK_LABEL (rf->label), _("Complete."));
                	if (rf->info->cancel_button)
                        	gtk_widget_set_sensitive(rf->info->cancel_button, FALSE);

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
        	g_set_error(&err, NET_ERROR, NET_ERROR_GENERIC,
                	soup_status_get_phrase(msg->status_code));
                gchar *tmsg = g_strdup_printf("\n%s\n%s", user_data, err->message);
                rss_error(user_data, NULL, _("Error fetching feed."), tmsg);
                g_free(tmsg);
        	goto out;
    	}

	if (rf->cancel)
	{
#ifdef EVOLUTION_2_12
		if(rf->label && rf->feed_queue == 0 && rf->info)
        	{
			farticle=0;
			ftotal=0;
                	gtk_label_set_markup (GTK_LABEL (rf->label), _("Canceled."));
                if (rf->info->cancel_button)
                        gtk_widget_set_sensitive(rf->info->cancel_button, FALSE);

                g_hash_table_steal(rf->info->data->active, rf->info->uri);
                rf->info->data->infos = g_list_remove(rf->info->data->infos, rf->info);

                if (g_hash_table_size(rf->info->data->active) == 0) {
                        if (rf->info->data->gd)
                                gtk_widget_destroy((GtkWidget *)rf->info->data->gd);
                }
		taskbar_op_finish("main");
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


	GString *response = g_string_new_len(msg->body, msg->length);

	g_print("feed %s\n", user_data);

	while (gtk_events_pending ())
            gtk_main_iteration ();

	RDF *r = g_new0 (RDF, 1);
        r->shown = TRUE;
        xmlSubstituteEntitiesDefaultValue = 1;
        r->cache = xml_parse_sux (response->str, response->len);
	if (rsserror) {
		xmlError *err = xmlGetLastError();
                gchar *tmsg = g_strdup_printf("\n%s\nInvalid feed: %s", user_data, err->message);
                rss_error(user_data, NULL, _("Error while parsing feed."), tmsg);
                g_free(tmsg);
		goto out;
	}

	if (msg->status_code == SOUP_STATUS_CANCELLED)
		goto out;

	if (!deleted)
	{
		if (!user_data || !lookup_key(user_data))
			goto out;
		r->uri =  g_hash_table_lookup(rf->hr, lookup_key(user_data));
	
        	chn_name = display_doc (r);

		if (chn_name)
		{
			if (g_ascii_strcasecmp(user_data, chn_name) != 0)
			{
				gchar *md5 = g_strdup(
					g_hash_table_lookup(rf->hrname, user_data));
				g_hash_table_remove(rf->hrname_r, md5);
				g_hash_table_remove(rf->hrname, user_data);
				g_hash_table_insert(rf->hrname, g_strdup(chn_name), md5);
				g_hash_table_insert(rf->hrname_r, g_strdup(md5), 
								g_strdup(chn_name));
				save_gconf_feed();
				update_ttl(md5, r->ttl);
				user_data = chn_name;
			}
		}
		if (r->cache)
			xmlFreeDoc(r->cache);
		if (r->type)
			g_free(r->type);
		if (r->version)
			g_free(r->version);
	}
	//ftotal+=r->total;
	update_sr_message();
	g_free(r);
	g_string_free(response, 1);

	if (!deleted)
	{
		if (g_hash_table_lookup(rf->hrdel_feed, lookup_key(user_data)))
			get_feed_age(user_data, lookup_key(user_data));
	}
//tout:	

#ifdef EVOLUTION_2_12
	if (rf->sr_feed && !deleted)
	{
		gchar *furl = g_strdup_printf("<b>%s</b>: %s", _("Feed"), user_data);
		gtk_label_set_markup (GTK_LABEL (rf->sr_feed), furl);
		gtk_label_set_justify(GTK_LABEL (rf->sr_feed), GTK_JUSTIFY_LEFT);
		g_free(furl);
	}
	if(rf->label && rf->feed_queue == 0 && rf->info)
	{
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
		taskbar_op_finish("main");
		//clean data that might hang on rf struct
		rf->sr_feed = NULL;
		rf->label = NULL;
		flabel = NULL;
		rf->progress_bar = NULL;
		rf->info = NULL;
	}
#endif
out:	
	if (user_data)
	{
		//not sure why it dies here
		if (!rf->cancel && !rf->cancel_all)
			g_free(user_data);
	}
	return;
}

void
fetch_feed(gpointer key, gpointer value, gpointer user_data)
{ 
	GError *err = NULL;
	GString *content;
	GString *post;
	GtkWidget *ed;
	RDF *r;


	//exclude feeds that have special update interval or 
	//no update at all
	if (GPOINTER_TO_INT(g_hash_table_lookup(rf->hrupdate, lookup_key(key))) >= 2
	&& !force_update)
		return;

	// check if we're enabled and no cancelation signal pending
	// and no imports pending
	if (g_hash_table_lookup(rf->hre, lookup_key(key)) && !rf->cancel && !rf->import)
	{
		d(g_print("\nFetching: %s..%s\n", 
			g_hash_table_lookup(rf->hr, lookup_key(key)), key));
		rf->feed_queue++;

		fetch_unblocking(
				g_hash_table_lookup(rf->hr, lookup_key(key)),
				user_data,
				key,
				(gpointer)finish_feed,
				g_strdup(key),	// we need to dupe key here
				1,
				&err);			// because we might lose it if
							// feed gets deleted
		if (err)
		{
			rf->feed_queue--;
                     	gchar *msg = g_strdup_printf("\n%s\n%s", 
				 	key, err->message);
                        rss_error(key, NULL, _("Error fetching feed."), msg);
                     	g_free(msg);
		}
		
	}
	else if (rf->cancel && !rf->feed_queue)
		rf->cancel = 0;		//all feeds where either procesed or skipped
}

void
#if LIBSOUP_VERSION < 2003000
finish_website (SoupMessage *msg, gint user_data)
#else
finish_website (SoupSession *soup_sess, SoupMessage *msg, gint user_data)
#endif
{
	GString *response = g_string_new_len(msg->response_body->data, msg->response_body->length);
	g_print("browser full:%d\n", response->len);
	g_print("browser fill:%d\n", browser_fill);
	g_print("browser fill:%d%%\n", (browser_fill*100)/response->len);
	gchar *str = (response->str)+user_data;
	gint len = strlen(response->str)-browser_fill;
	g_print("len:%d\n", len);
	if (len>0) {
		browser_write(str, len);
		gtk_moz_embed_close_stream(GTK_MOZ_EMBED(rf->mozembed));
		g_string_free(response, 1);
//		gtk_widget_show(rf->mozembed);
	}
	browser_fill = 0;
}

void
#if LIBSOUP_VERSION < 2003000
finish_comments (SoupMessage *msg, gpointer user_data)
#else
finish_comments (SoupSession *soup_sess, SoupMessage *msg, gpointer user_data)
#endif
{
	guint reload=0;
	taskbar_op_set_progress("comments", "www", 0.01);

//	if (!msg->length)
	//	goto out;

//	if (msg->status_code == SOUP_STATUS_CANCELLED)
//		goto out;

	GString *response = g_string_new_len(msg->response_body->data, msg->response_body->length);

//#ifdef RSS_DEBUG
//	g_print("feed %s\n", user_data);
//#endif
	if (!commstream)
		reload = 1;

	commstream = response->str; 
	if (reload)
		em_format_redraw((EMFormat *)user_data);
	
	while (gtk_events_pending ())
            gtk_main_iteration ();
}

static void
refresh_cb (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	em_format_redraw((EMFormat *)pobject);
}

gchar *
print_comments(gchar *url, gchar *stream)
{
        RDF *r = NULL;
        r = g_new0 (RDF, 1);
        r->shown = TRUE;
	xmlDocPtr doc = NULL;
        xmlNodePtr root = NULL;
        xmlSubstituteEntitiesDefaultValue = 0;
        doc = xml_parse_sux (stream, strlen(stream));
//        d(g_print("content:\n%s\n", content->str));
        root = xmlDocGetRootElement(doc);

        if ((doc != NULL && root != NULL)
                && (strcasestr(root->name, "rss")
                || strcasestr(root->name, "rdf")
                || strcasestr(root->name, "feed"))) {
                r->cache = doc;
                r->uri = url;

                return display_comments (r);
	}
}


void
fetch_comments(gchar *url, CamelStream *stream)
{
	GError *err = NULL;
	g_print("\nFetching comments from: %s\n", 
		url);

	fetch_unblocking(
				url,
				NULL,
				NULL,
				(gpointer)finish_comments,
				stream,	// we need to dupe key here
				1,
				&err);			// because we might lose it if
							// feed gets deleted
		if (err)
		{
                     	gchar *msg = g_strdup_printf("\n%s\n%s", 
				 	url, err->message);
                        rss_error(url, NULL, _("Error fetching feed."), msg);
                     	g_free(msg);
		}
}

gboolean
update_articles(gboolean disabler)
{
	MailComponent *mc = mail_component_peek ();
	g_print("stAte:%d\n", mc->priv->quit_state);
        if (mc->priv->quit_state != -1)
		rf->cancel=1;

	if (!rf->pending && !rf->feed_queue && !rf->cancel_all && rf->online)
	{
		g_print("Reading RSS articles...\n");
		rf->pending = TRUE;
		check_folders();
		rf->err = NULL;
		taskbar_op_message();
		network_timeout();
		g_hash_table_foreach(rf->hrname, fetch_feed, statuscb);	
		rf->pending = FALSE;
	}
	return disabler;
}

gchar *
rss_component_peek_base_directory(MailComponent *component)
{
/* http://bugzilla.gnome.org/show_bug.cgi?id=513951 */
#if (EVOLUTION_VERSION >= 22300)		// include devel too
	return g_strdup_printf("%s/rss",
		mail_component_peek_base_directory (component));
#else
	return g_strdup_printf("%s/mail/rss",
            	mail_component_peek_base_directory (component));
#endif
}

gchar *
get_main_folder(void)
{
	gchar mf[512];
	gchar *feed_dir = rss_component_peek_base_directory(mail_component_peek());
        if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
            g_mkdir_with_parents (feed_dir, 0755);
        gchar *feed_file = g_strdup_printf("%s/main_folder", feed_dir);
        g_free(feed_dir);
        if (g_file_test(feed_file, G_FILE_TEST_EXISTS))
	{
		FILE *f = fopen(feed_file, "r");
		if (f)
		{
			if (fgets(mf, 511, f) != NULL)
			{
				fclose(f);
				g_free(feed_file);
				return g_strdup(mf);
			}
		}
	}
	g_free(feed_file);
	return g_strdup(DEFAULT_FEEDS_FOLDER);
}

GHashTable *
get_feed_folders(void)
{
	gchar tmp1[512];
	gchar tmp2[512];
	
	rf->feed_folders = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	rf->reversed_feed_folders = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	gchar *feed_dir = rss_component_peek_base_directory(mail_component_peek());
        if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
            g_mkdir_with_parents (feed_dir, 0755);
        gchar *feed_file = g_strdup_printf("%s/feed_folders", feed_dir);
        g_free(feed_dir);
        if (g_file_test(feed_file, G_FILE_TEST_EXISTS))
	{
		FILE *f = fopen(feed_file, "r");
		while (!feof(f))
		{
			fgets(tmp1, 512, f);
			fgets(tmp2, 512, f);
			g_hash_table_insert(rf->feed_folders,
						g_strdup(g_strstrip(tmp1)),
						g_strdup(g_strstrip(tmp2)));
		}
		fclose(f);
	}
	g_free(feed_file);
	g_hash_table_foreach(rf->feed_folders, 
				(GHFunc)populate_reversed, 
				rf->reversed_feed_folders);
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

//
//lookups feed folder name
//this can be different from the default if folder was renamed
//

gchar *
lookup_feed_folder(gchar *folder)
{
	gchar *new_folder = g_hash_table_lookup(rf->reversed_feed_folders, folder);
	return new_folder ? new_folder : folder;
}

gpointer
lookup_chn_name_by_url(gchar *url)
{
	gpointer crc_feed = gen_md5(url);
        gpointer chn_name = g_hash_table_lookup(rf->hrname_r,
                        g_strdup(crc_feed));
	g_free(crc_feed);
	return chn_name;
}


void
#if LIBSOUP_VERSION < 2003000
finish_update_feed_image (SoupMessage *msg, gpointer user_data)
#else
finish_update_feed_image (SoupSession *soup_sess, SoupMessage *msg, gpointer user_data)
#endif
{
	xmlChar *icon = NULL;
	gchar *icon_url = NULL;
        gchar *feed_dir = rss_component_peek_base_directory(mail_component_peek());
        gchar *feed_file = g_strdup_printf("%s/%s.img", feed_dir, user_data);
        g_free(feed_dir);
	gchar *url = g_hash_table_lookup(rf->hr, user_data);
	gchar *urldir = g_path_get_dirname(url);
	gchar *server = get_server_from_uri(url);
	rfMessage *rfmsg = g_new0(rfMessage, 1);
	rfmsg->status_code = msg->status_code;
#if LIBSOUP_VERSION < 2003000
	rfmsg->body = msg->response.body;
	rfmsg->length = msg->response.length;
#else
	rfmsg->body = (gchar *)(msg->response_body->data);
	rfmsg->length = msg->response_body->length; 
#endif
	xmlChar *app;
	xmlNode *doc = (xmlNode *)parse_html_sux (rfmsg->body, rfmsg->length);
	while (doc) {
		doc = html_find(doc, "link");
                if (app = xmlGetProp(doc, "rel")) {
			if (!g_ascii_strcasecmp(app, "shorcut icon")
			|| !g_ascii_strcasecmp(app, "icon")) {
				icon = xmlGetProp(doc, "href");
				exit;
			}
	
		}
		xmlFree(app);
	}
	g_free(rfmsg);
	if (icon) {
		if (strstr(icon, "://") == NULL)
			icon_url = g_strconcat(server, "/", icon, NULL);
		else
			icon_url = icon;

		fetch_unblocking(
			icon_url,
			textcb,
			NULL,
			(gpointer)finish_create_image,
			g_strdup(feed_file),	// we need to dupe key here
			0,
//			&err);			// because we might lose it if
			NULL);
	} else {
                //              r->image = NULL;
		icon_url = g_strconcat(urldir, "/favicon.ico", NULL);
		fetch_unblocking(
				icon_url,
				textcb,
				NULL,
				(gpointer)finish_create_image,
				g_strdup(feed_file),	// we need to dupe key here
				0,
//				&err);			// because we might lose it if
				NULL);
		g_free(icon_url);
		icon_url = g_strconcat(server, "/favicon.ico", NULL);
		fetch_unblocking(
				icon_url,
				textcb,
				NULL,
				(gpointer)finish_create_image,
				g_strdup(feed_file),	// we need to dupe key here
				0,
//				&err);			// because we might lose it if
				NULL);
	}
	g_free(feed_file);
	g_free(icon_url);
	g_free(server);
	g_free(urldir);
}

void
update_feed_image(gchar *image, gchar *key)
{
        GError *err = NULL;
//	if (!image)
//		return;
  //      g_return_if_fail (image != NULL);
        gchar *feed_dir = rss_component_peek_base_directory(mail_component_peek());
        if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
            g_mkdir_with_parents (feed_dir, 0755);
        gchar *feed_file = g_strdup_printf("%s/%s.img", feed_dir, key);
        g_free(feed_dir);
        if (!g_file_test(feed_file, G_FILE_TEST_EXISTS)) {
	if (image) {		//we need to validate image here with load_pixbuf
		CamelStream *feed_fs = camel_stream_fs_new_with_name(feed_file,
			O_RDWR|O_CREAT, 0666);
                net_get_unblocking(image,
                                textcb,
                                NULL,
                                (gpointer)finish_image,
                                feed_fs,
                                0,
                                &err);
                if (err) {
			g_print("ERR:%s\n", err->message);
                	g_free(feed_file);
			return;
		}
	} else {
		gchar *url = g_hash_table_lookup(rf->hr, key);
		gchar *server = get_server_from_uri(url);
		fetch_unblocking(
			server,
			textcb,
			NULL,
			(gpointer)finish_update_feed_image,
			key,	// we need to dupe key here
			0,
			&err);			// because we might lose it if
        }
	}
}

void
update_main_folder(gchar *new_name)
{
	FILE *f;
	if (rf->main_folder)
		g_free(rf->main_folder);
	rf->main_folder = g_strdup(new_name);
	
	gchar *feed_dir = rss_component_peek_base_directory(mail_component_peek());
        if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
            g_mkdir_with_parents (feed_dir, 0755);
        gchar *feed_file = g_strdup_printf("%s/main_folder", feed_dir);
        g_free(feed_dir);
	if (f = fopen(feed_file, "w"))
        {
		fprintf(f, "%s", rf->main_folder);	
                fclose(f);
        }
	g_free(feed_file);
	
}

void
write_feeds_folder_line(gpointer key, gpointer value, FILE *file)
{
	feed_folders *ff = g_new0(feed_folders, 1);
	ff->rname = key;
	ff->oname = value;
	fprintf(file, "%s\n", key);
	fprintf(file, "%s\n", value);
}

void
populate_reversed(gpointer key, gpointer value, GHashTable *hash)
{
	g_hash_table_insert(hash, g_strdup(value), g_strdup(key));
}

/*construct feed_folders file with rename allocation
 * old_name initial channel name
 * new_name renamed name
 */

void
update_feed_folder(gchar *old_name, gchar *new_name)
{
	gchar *oname = extract_main_folder(old_name);
	gchar *nname = extract_main_folder(new_name);
	FILE *f;
	gchar *feed_dir = rss_component_peek_base_directory(mail_component_peek());
        if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
            g_mkdir_with_parents (feed_dir, 0755);
        gchar *feed_file = g_strdup_printf("%s/feed_folders", feed_dir);
        g_free(feed_dir);
	f = fopen(feed_file, "wb");
	if (!f)
		return;
	gchar *orig_name = g_hash_table_lookup(rf->feed_folders, oname);
	if (!orig_name)
		g_hash_table_replace(rf->feed_folders, g_strdup(nname), g_strdup(oname));
	else
	{
		g_hash_table_replace(rf->feed_folders, g_strdup(nname), g_strdup(orig_name));
		g_hash_table_remove(rf->feed_folders, oname);
	}

	g_hash_table_foreach(rf->feed_folders, 
				(GHFunc)write_feeds_folder_line, 
				(gpointer *)f);
	fclose(f);
	g_hash_table_destroy(rf->reversed_feed_folders);
	rf->reversed_feed_folders = g_hash_table_new_full(g_str_hash, 
							g_str_equal, 
							g_free, 
							g_free);
	g_hash_table_foreach(rf->feed_folders, 
				(GHFunc)populate_reversed, 
				rf->reversed_feed_folders);
	g_free(oname);
	g_free(nname);
}

CamelFolder *
check_feed_folder(gchar *folder_name)
{
	CamelStore *store = mail_component_peek_local_store(NULL);
	CamelFolder *mail_folder;
	gchar *main_folder = lookup_main_folder();
	gchar *real_folder = lookup_feed_folder(folder_name);
	gchar *real_name = g_strdup_printf("%s/%s", main_folder, real_folder);
	d(g_print("main_folder:%s\n", main_folder));
	d(g_print("real_folder:%s\n", real_folder));
	d(g_print("real_name:%s\n", real_name));
        mail_folder = camel_store_get_folder (store, real_name, 0, NULL);
        if (mail_folder == NULL)
        {
                camel_store_create_folder (store, main_folder, real_folder, NULL);
                mail_folder = camel_store_get_folder (store, real_name, 0, NULL);
        }
	g_free(real_name);
	return mail_folder;

}

static void
store_folder_deleted(CamelObject *o, void *event_data, void *data)
{
	CamelFolderInfo *info = event_data;
	printf("Folder deleted '%s'\n", info->name);
}

static void
store_folder_renamed(CamelObject *o, void *event_data, void *data)
{
	CamelRenameInfo *info = event_data;


	gchar *main_folder = lookup_main_folder();
	if (!g_ascii_strncasecmp(info->old_base, main_folder, strlen(main_folder)))
	{
		printf("Folder renamed to '%s' from '%s'\n", info->new->full_name, info->old_base);
		if (!g_ascii_strncasecmp(main_folder, info->old_base, strlen(info->old_base)))
			update_main_folder(info->new->full_name);
		else
			update_feed_folder(info->old_base, info->new->full_name);
	}
}

typedef struct custom_fetch_data {
	gboolean disabler;
	gpointer key;
	gpointer value;
	gpointer user_data;
} CDATA;

gboolean
custom_update_articles(CDATA *cdata)
{
	GError *err = NULL;
        GString *content;
        GString *post;
        GtkWidget *ed;
        RDF *r;
	//if (!rf->pending && !rf->feed_queue && rf->online)
	if (rf->online)
	{
		g_print("Fetch (custom) RSS articles...\n");
		check_folders();
		rf->err = NULL;
		//taskbar_op_message();
		network_timeout();
        	// check if we're enabled and no cancelation signal pending
        	// and no imports pending
        	if (g_hash_table_lookup(rf->hre, lookup_key(cdata->key)) && !rf->cancel && !rf->import)
        	{
                	d(g_print("\nFetching: %s..%s\n",
                 		g_hash_table_lookup(rf->hr, lookup_key(cdata->key)), cdata->key));
                	rf->feed_queue++;

                	net_get_unblocking(
                                       g_hash_table_lookup(rf->hr, lookup_key(cdata->key)),
                                       cdata->user_data,
                                       cdata->key,
                                       (gpointer)finish_feed,
                                       g_strdup(cdata->key),  // we need to dupe key here
                                       1,
                                       &err);                  // because we might lose it if
			if (err)
			{
				rf->feed_queue--;
                     		gchar *msg = g_strdup_printf("\n%s\n%s", 
				 	cdata->key, err->message);
                        	rss_error(cdata->key, NULL, _("Error fetching feed."), msg);
                     		g_free(msg);
			}
                                                               // feed gets deleted
		} 
 		else if (rf->cancel && !rf->feed_queue)
                	rf->cancel = 0;         //all feeds where either procesed or skipped
	}
	return TRUE;
}

void
custom_fetch_feed(gpointer key, gpointer value, gpointer user_data)
{ 
	guint time_id = 0;
	if (!custom_timeout)
		custom_timeout = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	if (GPOINTER_TO_INT(g_hash_table_lookup(rf->hrupdate, lookup_key(key))) == 2
	 && g_hash_table_lookup(rf->hre, lookup_key(key)))
	{
		d(g_print("custom key:%s\n", key));
		guint ttl = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrttl, lookup_key(key)));
		guint ttl_multiply = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrttl_multiply, lookup_key(key)));
		if (ttl) {
			CDATA *cdata = g_new0(CDATA, 1);
			cdata->key = key;
			cdata->value = value;
			cdata->user_data = user_data;
			time_id = GPOINTER_TO_INT(g_hash_table_lookup(custom_timeout,
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
			time_id = g_timeout_add (ttl * 60 * 1000 * ttl_multiply,
                           (GtkFunction) custom_update_articles,
                           cdata);
			g_hash_table_replace(custom_timeout, 
				g_strdup(lookup_key(key)), 
				GINT_TO_POINTER(time_id));
		}
	}
	
}

void gtkut_window_popup(GtkWidget *window)
{
        gint x, y, sx, sy, new_x, new_y;

        g_return_if_fail(window != NULL);
        g_return_if_fail(window->window != NULL);

        sx = gdk_screen_width();
        sy = gdk_screen_height();

        gdk_window_get_origin(window->window, &x, &y);
        new_x = x % sx; if (new_x < 0) new_x = 0;
        new_y = y % sy; if (new_y < 0) new_y = 0;
        if (new_x != x || new_y != y)
                gdk_window_move(window->window, new_x, new_y);

        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), FALSE);
        gtk_window_present(GTK_WINDOW(window));
#ifdef G_OS_WIN32
        /* ensure that the window is displayed at the top */
        gdk_window_show(window->window);
#endif
}

static void
icon_activated (GtkStatusIcon *icon, gpointer pnotify)
{
        GList *p, *pnext;
        for (p = (gpointer)evo_window; p != NULL; p = pnext) {
                pnext = p->next;

                if (gtk_window_is_active(GTK_WINDOW(p->data)))
		{
			g_print("window active\n");
                        gtk_window_iconify(GTK_WINDOW(p->data));
			gtk_window_set_skip_taskbar_hint(GTK_WINDOW(p->data), TRUE);
		}
		else
		{
                        gtk_window_iconify(GTK_WINDOW(p->data));
			gtkut_window_popup(GTK_WIDGET(p->data));
			gtk_window_set_skip_taskbar_hint(GTK_WINDOW(p->data), FALSE);
		}
        }
}

static void
create_status_icon(void)
{
	if (!status_icon) {
		gchar *iconfile = g_build_filename (EVOLUTION_ICONDIR,
	                                    "rss-icon-unread.png",
                                            NULL);

		status_icon = gtk_status_icon_new ();
        	gtk_status_icon_set_from_file (status_icon, iconfile);
		g_free(iconfile);
		g_signal_connect (G_OBJECT (status_icon), "activate", G_CALLBACK (icon_activated), NULL);
	}
   //     gtk_status_icon_set_visible (status_icon, FALSE);
}
        
gboolean
flicker_stop(gpointer user_data)
{
        gtk_status_icon_set_blinking (status_icon, FALSE);
	return FALSE;
}

void
flaten_status(gpointer msg, gpointer user_data)
{
	if (strlen(msg))
		if (flat_status_msg)
			flat_status_msg = g_strconcat(flat_status_msg, msg, NULL);
		else
			flat_status_msg = g_strconcat(msg, NULL);
}

static void
update_status_icon(const char *channel, gchar *title)
{
  	if (gconf_client_get_bool (rss_gconf, GCONF_KEY_STATUS_ICON, NULL)) {
		gchar *total = g_strdup_printf("%s: %s\n\n", channel, title);
		create_status_icon();
		g_queue_push_tail(status_msg, total);
		//g_free(total);
		if (g_queue_get_length(status_msg) == 6)
			g_queue_pop_head(status_msg);
		g_queue_foreach(status_msg, flaten_status, flat_status_msg);
		gtk_status_icon_set_tooltip (status_icon, flat_status_msg);
		gtk_status_icon_set_visible (status_icon, TRUE);
  		if (gconf_client_get_bool (rss_gconf, GCONF_KEY_BLINK_ICON, NULL)
	 	&& !gtk_status_icon_get_blinking(status_icon))
        		gtk_status_icon_set_blinking (status_icon, TRUE);
		g_timeout_add(15 * 1000, flicker_stop, NULL);
		g_free(flat_status_msg);
		flat_status_msg = NULL;
	}
}

static void
custom_feed_timeout(void)
{
	g_hash_table_foreach(rf->hrname, custom_fetch_feed, statuscb);
}

static void
store_folder_update(CamelObject *o, void *event_data, void *data)
{
	g_print("folder update\n");
}

static void
rss_online(CamelObject *o, void *event_data, void *data)
{
	rf->online =  camel_session_is_online (o);
}

struct __EShellPrivate {
        /* IID for registering the object on OAF.  */
        char *iid;

        GList *windows;

        /* EUriSchemaRegistry *uri_schema_registry; FIXME */
//        EComponentRegistry *component_registry;

        /* Names for the types of the folders that have maybe crashed.  */
        /* FIXME TODO */
        GList *crash_type_names; /* char * */

        /* Line status and controllers  */
//        EShellLineStatus line_status;
        int line_status_pending;
//        EShellLineStatus line_status_working;
  //      EvolutionListener *line_status_listener;

        /* Settings Dialog */
        union {
                GtkWidget *widget;
                gpointer pointer;
        } settings_dialog;

        /* If we're quitting and things are still busy, a timeout handler */
        guint quit_timeout;

        /* Whether the shell is succesfully initialized.  This is needed during
 *            the start-up sequence, to avoid CORBA calls to do make wrong things
 *                       to happen while the shell is initializing.  */
        unsigned int is_initialized : 1;

        /* Wether the shell is working in "interactive" mode or not.
 *            (Currently, it's interactive IIF there is at least one active
 *                       view.)  */
        unsigned int is_interactive : 1;

        /* Whether quit has been requested, and the shell is now waiting for
 *            permissions from all the components to quit.  */
        unsigned int preparing_to_quit : 1;
};
typedef struct __EShellPrivate EShellPrivate;

struct _EShell {
        BonoboObject parent;

        EShellPrivate *priv;
};
typedef struct _EShell EShell;

void get_shell(void *ep, ESEventTargetShell *t)
{
	EShell *shell = t->shell;
	EShellPrivate *priv = (EShellPrivate *)shell->priv;
	evo_window = (GtkWidget *)priv->windows;
}

void org_gnome_cooly_rss_startup(void *ep, EMPopupTargetSelect *t);

void org_gnome_cooly_rss_startup(void *ep, EMPopupTargetSelect *t)
{
  	if (gconf_client_get_bool (rss_gconf, GCONF_KEY_START_CHECK, NULL))
	{
		//as I don't know how to set this I'll setup a 10 secs timeout
		//and return false for disableation
		g_timeout_add (3 * 1000,
                           (GtkFunction) update_articles,
                           0);
	}
	gdouble timeout = gconf_client_get_float(rss_gconf, GCONF_KEY_REP_CHECK_TIMEOUT, NULL);
    	if (gconf_client_get_bool (rss_gconf, GCONF_KEY_REP_CHECK, NULL))
	{
		rf->rc_id = g_timeout_add (60 * 1000 * timeout,
                           (GtkFunction) update_articles,
                           (gpointer)1);
		
	}
	custom_feed_timeout();

	/* load transparency */
	gchar *pixfile = g_build_filename (EVOLUTION_ICONDIR,
                                            "pix.png",
                                                NULL);
	g_file_load_contents (g_file_parse_name(pixfile),
                                                         NULL,
                                                         &pixfilebuf,
                                                         &pixfilelen,
                                                         NULL,
                                                         NULL);
	g_free(pixfile);

        /* hook in rename event to catch feeds folder rename */
	CamelStore *store = mail_component_peek_local_store(NULL);
	camel_object_hook_event(store, "folder_renamed",
                                (CamelObjectEventHookFunc)store_folder_renamed, NULL);
	camel_object_hook_event(store, "folder_deleted",
                                (CamelObjectEventHookFunc)store_folder_deleted, NULL);
	camel_object_hook_event((void *)mail_component_peek_session(NULL),
				 "online", rss_online, NULL);
}

/* check if rss folders exists and create'em otherwise */
void
check_folders(void)
{
        CamelException ex;
	CamelStore *store = mail_component_peek_local_store(NULL);
	//I'm not sure folder name can be translatable
	CamelFolder *mail_folder = camel_store_get_folder (store, lookup_main_folder(), 0, NULL);
	if (mail_folder == NULL)
	{
		camel_store_create_folder (store, NULL, lookup_main_folder(), &ex);
	}
	camel_object_unref (mail_folder);
}

void org_gnome_cooly_rss_refresh(void *ep, EMPopupTargetSelect *t);

gboolean 
check_if_enabled (gpointer key, gpointer value, gpointer user_data)
{
	return GPOINTER_TO_INT(value);
}

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

        if (!rf->setup || g_hash_table_size(rf->hrname)<1)
        {
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
        if (!rf->progress_dialog)
        {
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
        if (!rf->pending && !rf->feed_queue)
        {
                rf->pending = TRUE;
                check_folders();

                rf->err = NULL;
		force_update = 1;
		taskbar_op_message();
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
	g_print("OP STATUS\n");
	g_print("CANCEL!!!!\n");

        struct _send_info *info = data;

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
org_gnome_cooly_rss(void *ep, EMEventTargetSendReceive *t);
#else
org_gnome_cooly_rss(void *ep, EMPopupTargetSelect *t);
#endif

void
#ifdef EVOLUTION_2_12
org_gnome_cooly_rss(void *ep, EMEventTargetSendReceive *t)
#else
org_gnome_cooly_rss(void *ep, EMPopupTargetSelect *t)
#endif
{
	GtkWidget *readrss_dialog;
	GtkWidget *readrss_label;
	GtkWidget *readrss_progress;
	GtkWidget *label,*progress_bar, *cancel_button, *status_label;
	GtkWidget *recv_icon;

	rf->t = t;

	//no feeds enabled
	if (!g_hash_table_find(rf->hre, check_if_enabled, NULL))
		return;

	if (!rf->setup || g_hash_table_size(rf->hrname)<1)
	{
		taskbar_push_message(_("No RSS feeds configured!"));
		return;
	}

#ifdef EVOLUTION_2_12
	struct _send_info *info;
	struct _send_data *data = (struct _send_data *)t->data;


	g_signal_connect(data->gd, "response", G_CALLBACK(dialog_response), NULL);

        info = g_malloc0 (sizeof (*info));
//        info->type = type;
                        
        info->uri = g_strdup("feed"); //g_stddup

        info->cancel = camel_operation_new (my_op_status, info);
        info->state = SEND_ACTIVE;
//        info->timeout_id = g_timeout_add (STATUS_TIMEOUT, operation_status_timeout, info);
                        
        g_hash_table_insert (data->active, info->uri, info);
//        list = g_list_prepend (list, info);

	recv_icon = gtk_image_new_from_stock (
                        "rss-main", GTK_ICON_SIZE_LARGE_TOOLBAR);

	guint row = t->row;
	row+=2;
	t->row = row;

	gtk_table_resize(GTK_TABLE(t->table), t->row, 4);

        char *pretty_url = g_strdup ("RSS");
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

	readrss_dialog = e_error_new(NULL, "org-gnome-evolution-rss:readrss",
		_("Reading RSS feeds..."), NULL);

	g_signal_connect(readrss_dialog, "response", G_CALLBACK(readrss_dialog_cb), NULL);
	GtkWidget *label2 = gtk_label_new(NULL);
	readrss_label = gtk_label_new(_("Please wait"));
	if (!rf->progress_dialog)
	{
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
		rf->label	= label2;
		flabel		= label2;
	}
#endif
bail:	if (!rf->pending && !rf->feed_queue)
	{
		rf->pending = TRUE;
		check_folders();
	
		rf->err = NULL;
		force_update = 1;
		taskbar_op_message();
		network_timeout();
		g_hash_table_foreach(rf->hrname, fetch_feed, statuscb);	
		// reset cancelation signal
		if (rf->cancel)
			rf->cancel = 0;
		force_update = 0;
		rf->pending = FALSE;
	}
	

//camel_store_subscribe_folder (store, ->node->info->full_name, &mm->ex);
//camel_store_subscribe_folder (store, "www", NULL);
}

void
rss_finalize(void)
{
	g_print("RSS: cleaning all remaining sessions ..");
	abort_all_soup();
	g_print(".done\n");
	if (rf->mozembed)
		gtk_widget_destroy(rf->mozembed);

	guint render = GPOINTER_TO_INT(
		gconf_client_get_int(rss_gconf, 
			GCONF_KEY_HTML_RENDER, 
			NULL));
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
        guint engine = gconf_client_get_int(rss_gconf, GCONF_KEY_HTML_RENDER, NULL);
#if !defined(HAVE_GECKO) && !defined (HAVE_WEBKIT)
        engine=0;
#endif
if (engine == 2) {
#if !defined(HAVE_GECKO)
        engine=1;
#endif
}
if (engine == 1) {
#if !defined (HAVE_WEBKIT)
        engine=2;
#endif
}
	return engine;
#endif
	return 0;
}

int e_plugin_lib_enable(EPluginLib *ep, int enable);

int
e_plugin_lib_enable(EPluginLib *ep, int enable)
{
	if (enable) {
		bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
		bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
		rss_gconf = gconf_client_get_default();
		upgrade = 1;
		char *d;
        	d = getenv("RSS_VERBOSE_DEBUG");
        	if (d)
                	rss_verbose_debug = atoi(d);

		//initiate main rss structure
		if (!rf)
		{
			printf("RSS Plugin enabled (evolution %s, evolution-rss %s)\n",
				EVOLUTION_VERSION_STRING,
				VERSION);
			rf = malloc(sizeof(rssfeed));
			memset(rf, 0, sizeof(rssfeed));
			rf->setup = read_feeds(rf);
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
			rss_soup_init();
#if HAVE_DBUS
			d(g_print("init_dbus()\n"));
			/*D-BUS init*/
			rf->bus = init_dbus ();
#endif
			if (!rf->activity)	//keeping track of taskbar operations
				rf->activity = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
			if (!rf->error_hash)	//keeping trask of taskbar errors
				rf->error_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
			//there is no shutdown for e-plugin yet.
			atexit(rss_finalize);
			guint render = GPOINTER_TO_INT(
				gconf_client_get_int(rss_gconf, 
						GCONF_KEY_HTML_RENDER, 
						NULL));
		
			if (!render) 	// set render just in case it was forced in configure
			{
				render = RENDER_N;
  				gconf_client_set_int(rss_gconf, 
						GCONF_KEY_HTML_RENDER, render, NULL);
			}
#ifdef HAVE_GECKO
			if (2 == render)
				rss_mozilla_init();
#endif
		}
		upgrade = 2;
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

void e_plugin_lib_disable(EPluginLib *ep);

void
e_plugin_lib_disable(EPluginLib *ep)
{
	g_print("DIE!\n");
}

static void
free_filter_uids (gpointer user_data, GObject *ex_msg)
{
	g_print("weak unref called on filter_uids\n");
}

#ifdef _WIN32
#include "strptime.c"
#endif

void
create_mail(create_feed *CF)
{
	CamelFolder *mail_folder;
	CamelMimeMessage *new = camel_mime_message_new();
	CamelInternetAddress *addr;
	CamelMessageInfo *info;
	CamelException *ex;
	struct tm tm;
	time_t time;
	CamelDataWrapper *rtext;
	CamelContentType *type;
	CamelStream *stream;
	char *appended_uid = NULL;
	gchar *author = CF->q ? CF->q : CF->sender;

	mail_folder = check_feed_folder(CF->full_path);
	camel_object_ref(mail_folder);

        camel_folder_freeze(mail_folder);

	info = camel_message_info_new(NULL);
	camel_message_info_set_flags(info, CAMEL_MESSAGE_SEEN, 1);

	gchar *tmp = decode_entities(CF->subj);
	gchar *tmp2 = markup_decode(tmp);
	gchar *safe_subj = camel_header_encode_string(tmp2);
	camel_mime_message_set_subject(new, safe_subj);
	g_free(tmp);
	g_free(tmp2);

	addr = camel_internet_address_new(); 
	d(g_print("date:%s\n", CF->date));
   	camel_address_decode(CAMEL_ADDRESS(addr), author);
	camel_mime_message_set_from(new, addr);
	camel_object_unref(addr);

	int offset = 0;

	//handle pubdate
	if (CF->date)
	{
		//check if CF->date obeys rfc822
		if (!is_rfc822(CF->date))
			camel_mime_message_set_date(new, CAMEL_MESSAGE_DATE_CURRENT, 0);
		else
		{	
			time_t actual_time;
			actual_time = camel_header_decode_date(CF->date, &offset);
			camel_mime_message_set_date(new, actual_time, offset);
		}
	}
	else 
	{
		if (CF->dcdate)	//dublin core 
		{
			strptime(CF->dcdate, "%Y-%m-%dT%T%z", &tm);
			time = mktime(&tm);
			time_t actual_time = camel_header_decode_date (ctime(&time), &offset);
			camel_mime_message_set_date(new, actual_time, offset);
		}
		else /*use 'now' as time for failsafe*/
			camel_mime_message_set_date(new, CAMEL_MESSAGE_DATE_CURRENT, 0);
	}
	time = camel_mime_message_get_date (new, NULL) ;
	gchar *time_str = asctime(gmtime(&time));
	char *buf = g_strdup_printf("from %s by localhost via evolution-rss-%s with libsoup-%d; %s\r\n", "RSS", VERSION, LIBSOUP_VERSION, time_str);
	camel_medium_set_header(CAMEL_MEDIUM(new), "Received", buf);
	camel_medium_set_header(CAMEL_MEDIUM(new), "Website", CF->website);
	camel_medium_set_header(CAMEL_MEDIUM(new), "RSS-ID", CF->feedid);
	camel_medium_set_header(CAMEL_MEDIUM(new), "X-evolution-rss-feed-ID", g_strstrip(CF->feed_uri));
	if (CF->comments)
		camel_medium_set_header(CAMEL_MEDIUM(new), "X-evolution-rss-comments", CF->comments);
	if (CF->category) {
		GString *cats = g_string_new(NULL);
		GList *p;
		for (p = (GList *)CF->category; p != NULL; p=p->next) {
			if (p->next)
				g_string_append_printf(cats, "%s, ", p->data); 
			else
				g_string_append_printf(cats, "%s", p->data); 
		}
		camel_medium_set_header(CAMEL_MEDIUM(new), "X-evolution-rss-category", cats->str);
		g_string_free(cats, FALSE);
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
        camel_object_unref (stream);

	if (CF->encl)
	{
		CamelMultipart *mp = camel_multipart_new();
        	camel_multipart_set_boundary(mp, NULL);

		CamelMimePart *part = camel_mime_part_new();
	      	camel_medium_set_content_object((CamelMedium *)part, (CamelDataWrapper *)rtext);

		camel_multipart_add_part(mp, part);
		camel_object_unref(part);
		CamelMimePart *msgp = file_to_message(CF->encl);
		if (msgp)
		{
			camel_multipart_add_part(mp, msgp);
			camel_object_unref(msgp);
		}
	      	camel_medium_set_content_object((CamelMedium *)new, (CamelDataWrapper *)mp);
		camel_object_unref(mp);
	}
        else
		camel_medium_set_content_object(CAMEL_MEDIUM(new), CAMEL_DATA_WRAPPER(rtext));

	camel_folder_append_message(mail_folder, new, info, &appended_uid, ex);

	if (appended_uid != NULL)
	{
		filter_uids = g_ptr_array_sized_new(1);
		g_ptr_array_add(filter_uids, appended_uid);
		mail_filter_on_demand (mail_folder, filter_uids);
/*FIXME do not how to free this
		g_object_weak_ref((GObject *)filter_uids, free_filter_uids, NULL);*/
	}
	camel_folder_sync(mail_folder, FALSE, NULL);
	camel_folder_thaw(mail_folder);
        camel_operation_end(NULL);
	camel_object_unref(rtext);
	camel_object_unref(new);
	camel_message_info_free(info);
	camel_object_unref(mail_folder);
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
	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (g_file_test(filename, G_FILE_TEST_IS_REGULAR), NULL);
	const char *type;
        CamelStreamFs *file;
        CamelMimePart *msg = camel_mime_part_new();
	camel_mime_part_set_encoding(msg, CAMEL_TRANSFER_ENCODING_BINARY);
	CamelDataWrapper *content = camel_data_wrapper_new();
	
        //file = (CamelStreamFs *)camel_stream_fs_new_with_name(name, O_RDONLY, 0);
        file = (CamelStreamFs *)camel_stream_fs_new_with_name(filename, O_RDWR|O_CREAT, 0666);

	if (!file)
		return NULL;

        camel_data_wrapper_construct_from_stream(content, (CamelStream *)file);
        camel_object_unref((CamelObject *)file);
	camel_medium_set_content_object((CamelMedium *)msg, content);
        camel_object_unref(content);
	
	type = em_utils_snoop_type(msg);
	if (type)
		camel_data_wrapper_set_mime_type((CamelDataWrapper *)msg, type);

	gchar *tname = g_path_get_basename(filename);
	camel_mime_part_set_filename(msg, tname);
	g_free(tname);

        return msg;
}

void
print_cf(create_feed *CF)
{
	g_print("Sender: %s ", CF->sender);
	g_print("Subject: %s \n", CF->subj);
	g_print("Date: %s\n", CF->date);
	g_print("Feedid: %s\n", CF->feedid);
	g_print("==========================\n");
	g_print("Name: %s ", CF->feed_fname);
	g_print("URI: %s\n", CF->feed_uri);
	g_print("Path: %s\n", CF->full_path);
	g_print("Website: %s\n", CF->website);
	g_print("==========================\n");
	g_print("%s\n", CF->body);
	g_print("==========================\n");
	g_print("q: %s\n", CF->q);
	g_print("encl: %s\n", CF->encl);
	g_print("dcdate: %s\n", CF->dcdate);
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
	g_free(CF->feed_fname);
	g_free(CF->feed_uri);
	if (CF->category)
		g_list_free(CF->category);
	g_free(CF);
}

void
write_feed_status_line(gchar *file, gchar *needle)
{
	FILE *fw = fopen(file, "a+");
	if (fw)
	{
		fputs(g_strstrip(needle), fw);
		fputs("\n", fw);
		fclose(fw);
	}
}

//check if feed already exists in feed file
//and if not add it to the feed file
gboolean
feed_is_new(gchar *file_name, gchar *needle)
{
	gchar rfeed[513];
	memset(rfeed, 0, 512);
	FILE *fr = fopen(file_name, "r");
	int occ = 0;
	gchar *tmpneedle = NULL;
	gchar *port =  get_port_from_uri(needle);
	if (port && atoi(port) == 80) {
		gchar *tp = g_strconcat(":", port, NULL);
		g_free(port);
		tmpneedle = strextr(needle, tp);
		g_free(tp);
	} else
		tmpneedle = g_strdup(needle);

	if (fr)
	{
	    while (fgets(rfeed, 511, fr) != NULL)
	    {
		if (rfeed && strstr(rfeed, tmpneedle))
		{
			occ=1;
			break;
		}
	    }
	    fclose(fr);
	}
	g_free(tmpneedle);
	return occ;
}

static void
#if LIBSOUP_VERSION < 2003000
finish_enclosure (SoupMessage *msg, create_feed *user_data)
#else
finish_enclosure (SoupSession *soup_sess, SoupMessage *msg, create_feed *user_data)
#endif
{
	char *tmpdir = NULL;
	gchar *name = NULL;
	FILE *f;
	tmpdir = e_mkdtemp("evo-rss-XXXXXX");
        if ( tmpdir == NULL)
            return;
	name = g_build_filename(tmpdir, g_path_get_basename(user_data->encl), NULL);

	f = fopen(name, "wb+");
	if (f)
	{
#if LIBSOUP_VERSION < 2003000
		fwrite(msg->response.body, msg->response.length, 1, f);
#else
		fwrite(msg->response_body->data, msg->response_body->length, 1, f);
#endif
		fclose(f);
		//replace encl with filename generated
		g_free(user_data->encl);
		// this will be a weak ref and get feed by free_cf
		user_data->encl = name;
	}
	
	g_free(tmpdir);
	if (!feed_is_new(user_data->feed_fname, user_data->feed_uri))
	{
		create_mail(user_data);
		write_feed_status_line(user_data->feed_fname, user_data->feed_uri);
	}
	free_cf(user_data);
}

static void
#if LIBSOUP_VERSION < 2003000
finish_image (SoupMessage *msg, CamelStream *user_data)
#else
finish_image (SoupSession *soup_sess, SoupMessage *msg, CamelStream *user_data)
#endif
{
	g_print("finish_image:%d\n", msg->status_code);
	// we might need to handle more error codes here
	if (503 != msg->status_code && //handle this timedly fasion
	    404 != msg->status_code &&
	      7 != msg->status_code) {
#if LIBSOUP_VERSION < 2003000
		if (msg->response.body) {
			camel_stream_write(user_data, msg->response.body, msg->response.length);
#else
		if (msg->response_body->data) {
			camel_stream_write(user_data, msg->response_body->data, msg->response_body->length);
#endif
			camel_stream_close(user_data);
			camel_object_unref(user_data);
		}
	} else { 
		camel_stream_write(user_data, pixfilebuf, pixfilelen);
		camel_stream_close(user_data);
		camel_object_unref(user_data);
	}
}

static void
#if LIBSOUP_VERSION < 2003000
finish_create_image (SoupMessage *msg, gchar *user_data)
#else
finish_create_image (SoupSession *soup_sess, SoupMessage *msg, gchar *user_data)
#endif
{
	g_print("finish_image(): status:%d, user_data;%s\n", msg->status_code);
	if (404 != msg->status_code) {
		CamelStream *feed_fs = camel_stream_fs_new_with_name(user_data,
			O_RDWR|O_CREAT, 0666);
		finish_image(soup_sess, msg, feed_fs);
	}
	g_free(user_data);
}

#define CAMEL_DATA_CACHE_BITS (6)
#define CAMEL_DATA_CACHE_MASK ((1<<CAMEL_DATA_CACHE_BITS)-1)

static char *
data_cache_path(CamelDataCache *cdc, int create, const char *path, const char *key)
{
        char *dir, *real;
	char *tmp = NULL;
        guint32 hash;

        hash = g_str_hash(key);
        hash = (hash>>5)&CAMEL_DATA_CACHE_MASK;
        dir = alloca(strlen(cdc->path) + strlen(path) + 8);
        sprintf(dir, "%s/%s/%02x", cdc->path, path, hash);
        tmp = camel_file_util_safe_filename(key);
	if (!tmp)
		return NULL;
        real = g_strdup_printf("%s/%s", dir, tmp);
        g_free(tmp);

        return real;
}

// constructs url from @base in case url is relative
gchar *
fetch_image(gchar *url, gchar *link)
{
        GError *err = NULL;
	gchar *tmpdir = NULL;
	gchar *name = NULL;
	CamelStream *stream = NULL;
	gchar *tmpurl = NULL;
	if (!url)
		return NULL;
	if (strstr(url, "://") == NULL) {
		if (*url == '/') {
		tmpurl = g_strconcat(get_server_from_uri(link), "/", url, NULL);
		g_print("fetch_image() tmpurl:%s\n", tmpurl);
		}
		if (*url == '.')
			tmpurl = g_strconcat(g_path_get_dirname(link), "/", url, NULL);
	} else {
		tmpurl = g_strdup(url);
	}
	gchar *feed_dir = g_build_path("/", rss_component_peek_base_directory(mail_component_peek()), "static", NULL);
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
	    g_mkdir_with_parents (feed_dir, 0755);
	http_cache = camel_data_cache_new(feed_dir, 0, NULL);
	g_free(feed_dir);
	stream = camel_data_cache_get(http_cache, HTTP_CACHE_PATH, tmpurl, NULL);
	if (!stream) {
		g_print("image cache MISS\n");
		stream = camel_data_cache_add(http_cache, HTTP_CACHE_PATH, tmpurl, NULL);
	} else 
		g_print("image cache HIT\n");


	net_get_unblocking(tmpurl,
                       	        textcb,
                               	NULL,
                               	(gpointer)finish_image,
                               	stream,
				0,
                               	&err);
	if (err) return NULL;
	gchar *result = data_cache_path(http_cache, FALSE, HTTP_CACHE_PATH, tmpurl);
	g_free(tmpurl);
	return result;
}

//migrates old feed data files from crc naming
//to md5 naming while preserving content
//
//this will be obsoleted over a release or two

void
migrate_crc_md5(const char *name, gchar *url)
{
	uint32_t crc = gen_crc(name);
	uint32_t crc2 = gen_crc(url);
	gchar *md5 = gen_md5(url);

	gchar *feed_dir = rss_component_peek_base_directory(mail_component_peek());
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
	    g_mkdir_with_parents (feed_dir, 0755);

	gchar *md5_name = g_strdup_printf("%s/%s", feed_dir, md5);
	gchar *feed_name = g_strdup_printf("%s/%x", feed_dir, crc);

	if (g_file_test(feed_name, G_FILE_TEST_EXISTS))
	{
		FILE *fr = fopen(feed_name, "r");
		FILE *fw = fopen(md5_name, "a+");
		gchar rfeed[513];
		memset(rfeed, 0, 512);
		int occ = 0;
		if (fr && fw)
		{
		    while (fgets(rfeed, 511, fr) != NULL)
		    {
		    	(void)fseek(fw, 0L, SEEK_SET);
			fwrite(rfeed, strlen(rfeed), 1, fw);
		    }
		    fclose(fw);
		    unlink(feed_name);
		}
		fclose(fr);

	}
	g_free(feed_name);
	feed_name = g_strdup_printf("%s/%x", feed_dir, crc2);
	if (g_file_test(feed_name, G_FILE_TEST_EXISTS))
	{
		FILE *fr = fopen(feed_name, "r");
		FILE *fw = fopen(md5_name, "a+");
		gchar rfeed[513];
		memset(rfeed, 0, 512);
		int occ = 0;
		if (fr && fw)
		{
		    while (fgets(rfeed, 511, fr) != NULL)
		    {
		    	(void)fseek(fw, 0L, SEEK_SET);
			fwrite(rfeed, strlen(rfeed), 1, fw);
		    }
		    fclose(fw);
		    unlink(feed_name);
		}
		fclose(fr);

	}

	g_free(feed_name);
	g_free(feed_dir);
	g_free(md5_name);
	g_free(md5);
}

gchar *
decode_utf8_entities(gchar *str)
{
	guint inlen, utf8len;
	gchar *buffer;
	g_return_if_fail (str != NULL);

	inlen = strlen(str);
	utf8len = 5*inlen+1;
	buffer = g_malloc0(utf8len);
	UTF8ToHtml(buffer, &utf8len, str, &inlen);
	return buffer;
}

gchar *
decode_entities(gchar *source)
{
 	GString *str = g_string_new(NULL);
 	GString *res = g_string_new(NULL);
 	gchar *string, *result;
        const unsigned char *s;
        guint len;
	int in, out;
	int state, pos;

	g_string_append(res, source);
reent:	s = (const unsigned char *)res->str;
        len = strlen(res->str);
	state = 0;
	pos = 1;
	g_string_truncate(str, 0);
	while (*s != 0 || len) {
		if (state) {
			if (*s==';') {
				state = 2; //entity found
				out = pos;
				break;
			} else {
				g_string_append_c(str, *s);
			}
		}
		if (*s=='&') {
			in = pos-1;
			state = 1;
                }
		*s++;
		pos++;
		len--;
	}
	if (state == 2) {
		htmlEntityDesc *my = (htmlEntityDesc *)htmlEntityLookup((xmlChar *)str->str);
		if (my) {
			g_string_erase(res, in, out-in);
			g_string_insert_unichar(res, in, my->value);
			gchar *result = res->str;
			g_string_free(res, FALSE);
			res = g_string_new(NULL);
			g_string_append(res, result);
			goto reent;
		}
	}
	result = res->str;
	g_string_free(res, FALSE);
	return result;
}	

gchar *
decode_html_entities(gchar *str)
{
	gchar *newstr;
	g_return_if_fail (str != NULL);

	xmlParserCtxtPtr ctxt = xmlNewParserCtxt();
	xmlCtxtUseOptions(ctxt,   XML_PARSE_RECOVER
				| XML_PARSE_NOENT
				| XML_PARSE_NOERROR
				| XML_PARSE_NONET);

	xmlChar *tmp =  (gchar *)xmlStringDecodeEntities(ctxt,
					     BAD_CAST str,
					     XML_SUBSTITUTE_REF
					     & XML_SUBSTITUTE_PEREF,
					     0,
					     0,
					     0);

	newstr = g_strdup(tmp);
	xmlFree(tmp);
	xmlFreeParserCtxt(ctxt);
	return newstr;
}

gchar *
encode_html_entities(gchar *str)
{
        gchar *newstr;
        g_return_if_fail (str != NULL);

/*        xmlParserCtxtPtr ctxt = xmlNewParserCtxt();
        xmlCtxtUseOptions(ctxt,   XML_PARSE_RECOVER
                                | XML_PARSE_NOENT
                                | XML_PARSE_NOERROR
                                | XML_PARSE_NONET);*/

        xmlChar *tmp =  (gchar *)xmlEncodeEntitiesReentrant(NULL, str);

/*        xmlChar *tmp =  (gchar *)xmlStringDecodeEntities(ctxt,
                                             BAD_CAST str,
                                             XML_SUBSTITUTE_REF
                                             & XML_SUBSTITUTE_PEREF,
                                             0,
                                             0,
                                             0);

        newstr = g_strdup(tmp);
        xmlFree(tmp);
        xmlFreeParserCtxt(ctxt);
        return newstr;*/
	return tmp;
}

gchar *
encode_rfc2047(gchar *str)
{
	gchar *tmp = decode_entities(str);
        gchar *rfctmp = camel_header_encode_string(tmp);
        g_free(tmp);
	return rfctmp;
}

static gchar *
update_comments(RDF *r)
{
        guint i;
        create_feed *CF;
        xmlNodePtr el;
        GString *comments = g_string_new(NULL);
        for (i=0; NULL != (el = g_array_index(r->item, xmlNodePtr, i)); i++) {
                CF = parse_channel_line(el->children, NULL, NULL);
        //print_cf(CF);
		g_string_append_printf (comments,
                        "<div style=\"border: solid #%06x 1px; background-color: #%06x; padding: 0px; color: #%06x;\">",
                        frame_colour & 0xffffff, content_colour & 0xEDECEB & 0xffffff, text_colour & 0xffffff);
                g_string_append_printf (comments,
                        "<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
                        "<a href=%s><b>%s</b></a> on %s</div>",
			content_colour & 0xEDECEB & 0xffffff, text_colour & 0xffffff,
				CF->website, CF->subj, CF->date);
                g_string_append_printf (comments, 
				"<div style=\"border: solid #%06x 0px; background-color: #%06x; padding: 10px; color: #%06x;\">"
                                "%s</div>",
                        	frame_colour & 0xffffff, content_colour & 0xffffff, text_colour & 0xffffff,
                                CF->body);
                g_string_append_printf(comments, "</div>&nbsp;");
        }
	commcnt=i;
        gchar *scomments=comments->str;
        g_string_free(comments, FALSE);
        return scomments;
}

gchar *
display_comments (RDF *r)
{
	xmlNodePtr root = xmlDocGetRootElement (r->cache);
	if (tree_walk (root, r)) {
		gchar *comments = update_comments(r);
		if (r->maindate)
			g_free(r->maindate);
		g_array_free(r->item, TRUE);
		return comments;
	}
	return NULL;
}


gchar *
display_doc (RDF *r)
{
	xmlNodePtr root = xmlDocGetRootElement (r->cache);
	if (tree_walk (root, r)) {
		update_feed_image(r->image, gen_md5(r->uri));
		r->feedid = update_channel(r);
		if (r->maindate)
			g_free(r->maindate);
		g_array_free(r->item, TRUE);
		g_free(r->feedid);
		return r->title;
	}
	return NULL;
}

static void
delete_oldest_article(CamelFolder *folder, guint unread)
{
	CamelMessageInfo *info;
	GPtrArray *uids;
	guint i, j = 0, imax = 0;
	guint q = 0;
	guint w = 0;
	guint32 flags;
	time_t date, min_date = 0;
	uids = camel_folder_get_uids (folder);
       	for (i = 0; i < uids->len; i++)
	{
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
       			if (flags & CAMEL_MESSAGE_SEEN)
			{
				if (!j)
				{
					min_date = date;
					imax = i;
					j++;
				}
				if (date < min_date)
				{
					imax = i;
					min_date = date;
				}
			}
			else		//UNSEEN
			{
				if (unread)
				{
					if (!q)
					{
                                       		min_date = date;
						imax = i;
						q++;
					}
                               		if (date < min_date)
                               		{
                                       		imax = i;
                                       		min_date = date;
                               		}
				}
			}
               	}
		d(g_print("uid:%d j:%d/%d, date:%d, imax:%d\n", i, j, q, min_date, imax));
out:          	camel_message_info_free(info);
	}
       	camel_folder_freeze(folder);
	if (min_date)
	{
		camel_folder_delete_message (folder, uids->pdata[imax]);
	}
       	camel_folder_thaw(folder);
	while (gtk_events_pending())
                  gtk_main_iteration ();
       	camel_folder_free_uids (folder, uids);
}

void
get_feed_age(gpointer key, gpointer value)
{
	CamelMessageInfo *info;
        CamelFolder *folder;
	CamelStore *store = mail_component_peek_local_store(NULL);
	GPtrArray *uids;
	time_t date, now;
	char strbuf[200];
	char strbuf2[200];
	guint i,total;
	guint32 flags;

	gchar *real_folder = lookup_feed_folder(key);
	d(g_print("Cleaning folder: %s\n", real_folder));

        gchar *real_name = g_strdup_printf("%s/%s", lookup_main_folder(), real_folder);
	if (!(folder = camel_store_get_folder (store, real_name, 0, NULL)))
                        goto fail;
	time (&now);
	
	guint del_unread = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_unread, value));
	guint del_feed = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_feed, value));
	inhibit_read = 1;
	if (del_feed == 2)
	{	
		guint del_days = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_days, value));
		uids = camel_folder_get_uids (folder);
        	camel_folder_freeze(folder);
        	for (i = 0; i < uids->len; i++)
		{
			info = camel_folder_get_message_info(folder, uids->pdata[i]);
                	if (info && rf->current_uid && strcmp(rf->current_uid, uids->pdata[i])) {
				date = camel_message_info_date_sent(info);
				if (date < now - del_days * 86400)
				{
					flags = camel_message_info_flags(info);
                               		if (!(flags & CAMEL_MESSAGE_SEEN))
					{
						if ((del_unread) && !(flags & CAMEL_MESSAGE_FLAGGED))
						{
							//camel_message_info_set_flags(info, CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_DELETED, ~0);
							camel_folder_delete_message(folder, uids->pdata[i]);
						}
					}
					else
						if (!(flags & CAMEL_MESSAGE_FLAGGED))
						{
							//camel_message_info_set_flags(info, CAMEL_MESSAGE_DELETED, ~0);
							camel_folder_delete_message(folder, uids->pdata[i]);
						}
				}
                        	camel_folder_free_message_info(folder, info);
                	}
		}
        	camel_folder_free_uids (folder, uids);
        	camel_folder_sync (folder, TRUE, NULL);
        	camel_folder_thaw(folder);
      		camel_folder_expunge (folder, NULL);
	}
	if (del_feed == 1)
	{
		guint del_messages = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_messages, value));
		guint total = camel_folder_get_message_count(folder);
		i=1;
		while (del_messages < camel_folder_get_message_count(folder) - camel_folder_get_deleted_message_count(folder) && i <= total)
		{
			delete_oldest_article(folder, del_unread);
			i++;
		}
	     	camel_folder_sync (folder, TRUE, NULL);
      		camel_folder_expunge (folder, NULL);
	}
	total = camel_folder_get_message_count (folder);
	camel_object_unref (folder);
	d(g_print("delete => remaining total:%d\n", total));
fail:	g_free(real_name);
	inhibit_read = 0;
}

