/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2008 Lucian Langa <cooly@gnome.eu.org> 
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

#define d(x)

#include <string.h>
#include <stdio.h>
#include <time.h>

#include <camel/camel-mime-message.h>
#include <camel/camel-folder.h>
#include <camel/camel-exception.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream-mem.h>

#include <mail/em-popup.h>
#include <e-util/e-error.h>
#include <e-util/e-icon-factory.h>

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
//////////////////////////////////
#include <mail/mail-component.h>
//////////////////////////////////

#include <misc/e-activity-handler.h>

#include <mail/em-format-html.h>

#include <mail/em-format.h>
#include <mail/em-format-hook.h>

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h> 
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <bonobo/bonobo-shlib-factory.h>

#include <glade/glade-xml.h>
#include <glade/glade.h>
#include <shell/evolution-config-control.h>
#include <shell/e-component-view.h>///

#include <libxml/parserInternals.h>
#include <libxml/xmlmemory.h>
#include <libxml/HTMLparser.h>

#ifdef HAVE_RENDERKIT

#ifdef HAVE_GTKMOZEMBED
#include <gtkmozembed.h>
#endif

#ifdef HAVE_WEBKIT
#include "webkitgtkglobal.h"
#include "webkitgtkpage.h"
#endif

#endif

#include <errno.h>

#include <libsoup/soup.h>
#if LIBSOUP_VERSION < 2003000
#include <libsoup/soup-message-queue.h>
#endif

#include "rss.h"
#include "network-soup.c"
#include "misc.c"
#if HAVE_DBUS
#include "dbus.c"
#endif
#include "rss-config-factory.c"

int pop = 0;
//#define RSS_DEBUG 1

#define DEFAULT_FEEDS_FOLDER "News&Blogs"
#define DEFAULT_NO_CHANNEL "Untitled channel"

/* ms between status updates to the gui */
#define STATUS_TIMEOUT (250)

static volatile int org_gnome_rss_controls_counter_id = 0;

struct _org_gnome_rss_controls_pobject {
        EMFormatHTMLPObject object;

        CamelMimePart *part;
        EMFormatHTML *format;
	GtkWidget *html;
	GtkWidget *container;
	CamelStream *stream;
	gchar *website;
	guint is_html;
	gchar *mem;
	guint shandler;		//mycall handler_id
};

/*struct _GtkHTMLEmbedded {
        HTMLObject object;

        gchar *name;
        gchar *value;
        HTMLForm *form;
        GtkWidget *widget, *parent;
        gint width, height;

        gint abs_x, abs_y;
        guint changed_id;
};*/

extern int xmlSubstituteEntitiesDefaultValue;

rssfeed *rf = NULL;

gboolean setup_feed(add_feed *feed);
gchar *display_doc (RDF *r);
void check_folders(void);
//u_int32_t 
gchar *
update_channel(const char *chn_name, char *url, char *main_date, GArray *item);
static char *layer_find (xmlNodePtr node, char *match, char *fail);
static char *layer_find_innerelement (xmlNodePtr node, char *match, char *el, char *fail);
static gchar *layer_find_innerhtml (xmlNodePtr node, char *match, char *submatch, char *fail);
xmlNodePtr layer_find_pos (xmlNodePtr node, char *match, char *submatch);
gchar *strplchr(gchar *source);
static char *gen_md5(gchar *buffer);
CamelMimePart *file_to_message(const char *name);
gchar *get_real_channel_name(gchar *uri, gchar *failed);
void save_gconf_feed(void);
void check_feed_age(void);
static gboolean check_if_match (gpointer key, gpointer value, gpointer user_data);
static void del_days_cb (GtkWidget *widget, add_feed *data);
static void del_messages_cb (GtkWidget *widget, add_feed *data);
void get_feed_age(gpointer key, gpointer value);
gboolean cancel_soup_sess(gpointer key, gpointer value, gpointer user_data);

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

/*======================================================================*/

static void
hash_check_value(gpointer key, gpointer value, gpointer user_data)
{
	if (value) rf->fe = 1;
}

gboolean
feeds_enabled(void)
{
	gboolean res = 0;
	rf->fe = 0;
	g_hash_table_foreach(rf->hre, hash_check_value, NULL);
	if (rf->fe)
	{
		res = rf->fe;
		rf->fe = 0;
	}
	return res;
}

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
//        		e_activity_handler_operation_set_error (activity_handler, activity_id, ed);
        		guint id = e_activity_handler_make_error (activity_handler, mail_component_peek(), msg, ed);
			g_hash_table_insert(rf->error_hash, newkey, id);
		}
		taskbar_op_finish(key);
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
	static GdkPixbuf *progress_icon = NULL;
	EActivityHandler *activity_handler = mail_component_peek_activity_handler (mail_component_peek ());
	progress_icon = e_icon_factory_get_icon ("mail-unread", E_ICON_SIZE_MENU);
//	progress_icon = NULL;
	char *mcp = g_strdup_printf("%p", mail_component_peek());
	guint activity_id = 
#if (EVOLUTION_VERSION >= 22200)
		e_activity_handler_cancelable_operation_started(activity_handler, "evolution-mail",
						progress_icon, message, TRUE,
						cancel_active_op, key);
#else
		e_activity_handler_operation_started(activity_handler, mcp,
						progress_icon, message, FALSE);
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
		e_activity_handler_operation_finished(activity_handler, activity_key);
		g_hash_table_remove(rf->activity, key);
	}
}

static void
statuscb(NetStatusType status, gpointer statusdata, gpointer data)
{
//	rssfeed *rf = data;
    NetStatusProgress *progress;
    float fraction = 0;
#ifdef RSS_DEBUG
	g_print("status:%d\n", status);
#endif

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
			gchar *furl;
			gchar *type = g_hash_table_lookup(rf->hrt, lookup_key(data));
			if (strncmp(type, "-", 1) == 0)
				furl = g_strdup_printf("<b>%s</b>: %s", 
					"RSS",data);
			else
				furl = g_strdup_printf("<b>%s</b>: %s", 
					type, data);
			gtk_label_set_markup (GTK_LABEL (rf->sr_feed), furl);
			g_free(furl);
		}
#endif
		gchar *tmsg;
		gchar *type = g_hash_table_lookup(rf->hrt, lookup_key(data));
        	if (strncmp(type, "-",1) == 0)
                        tmsg = g_strdup_printf("Fetching %s: %s", 
                                        "RSS", data);
        	else
                        tmsg = g_strdup_printf("Fetching %s: %s", 
                        type, data);
		taskbar_op_set_progress(data, tmsg, (gdouble)fraction);
		g_free(tmsg);
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
#ifdef RSS_DEBUG
	g_print("%f.", fraction*100);
#endif
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
  guint resp;

  if (!rf->hruser)
        rf->hruser = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
  if (!rf->hrpass)
        rf->hrpass = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

  GtkAccelGroup *accel_group = gtk_accel_group_new ();

  dialog1 = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog1), _("Enter User/Pass for Feed"));
  gtk_window_set_type_hint (GTK_WINDOW (dialog1), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_modal (GTK_WINDOW (dialog1), FALSE);

  dialog_vbox1 = GTK_DIALOG (dialog1)->vbox;
  gtk_widget_show (dialog_vbox1);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 3);

  table1 = gtk_table_new (2, 2, FALSE);
  gtk_widget_show (table1);
  gtk_box_pack_start (GTK_BOX (vbox1), table1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 10);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table1), 5);

  label1 = gtk_label_new (_("Username:"));
  gtk_widget_show (label1);
  gtk_table_attach (GTK_TABLE (table1), label1, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label1), 0, 0.5);

  label2 = gtk_label_new (_("Password:"));
  gtk_widget_show (label2);
  gtk_table_attach (GTK_TABLE (table1), label2, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label2), 0, 0.5);

  username = gtk_entry_new ();
  gtk_widget_show (username);
  gtk_table_attach (GTK_TABLE (table1), username, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_entry_set_invisible_char (GTK_ENTRY (username), 8226);
    gchar *user = g_hash_table_lookup(rf->hruser,  url);
	g_print("user:%s\n", user);
    if (user)
        gtk_entry_set_text (GTK_ENTRY (username), user);
  password = gtk_entry_new ();
  gtk_widget_show (password);
  gtk_table_attach (GTK_TABLE (table1), password, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_entry_set_visibility (GTK_ENTRY (password), FALSE);
  gchar *pass = g_hash_table_lookup(rf->hrpass,  url);
    if (pass)
        gtk_entry_set_text (GTK_ENTRY (password), pass);
  gtk_entry_set_invisible_char (GTK_ENTRY (password), 8226);

  checkbutton1 = gtk_check_button_new_with_mnemonic (_("Remember password"));
  gtk_widget_show (checkbutton1);
  gtk_box_pack_start (GTK_BOX (vbox1), checkbutton1, FALSE, FALSE, 0);


  dialog_action_area1 = GTK_DIALOG (dialog1)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);

  cancelbutton1 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton1);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog1), cancelbutton1, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton1, GTK_CAN_DEFAULT);

  okbutton1 = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton1);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog1), okbutton1, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton1, GTK_CAN_DEFAULT);
  gtk_widget_add_accelerator (okbutton1, "activate", accel_group,
                              GDK_Return, (GdkModifierType) 0,
                              GTK_ACCEL_VISIBLE);
  gtk_window_add_accel_group (GTK_WINDOW (dialog1), accel_group);
  gint result = gtk_dialog_run(GTK_DIALOG(dialog1));
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
	
        gtk_widget_destroy (dialog1);
	resp = 0;
        break;
    default:
        gtk_widget_destroy (dialog1);
	resp = 1;
        break;
  }
	return resp;
}

gboolean
cancel_soup_sess(gpointer key, gpointer value, gpointer user_data)
{
#if RSS_DEBUG 
	g_print("key:%p\n", key);
#endif

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

void
abort_all_soup(void)
{
	//abort all session
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
		if (SOUP_IS_MESSAGE(rf->b_msg_session))
		{
#if LIBSOUP_VERSION < 2003000
			soup_message_set_status(rf->b_msg_session, SOUP_STATUS_CANCELLED);
			soup_session_cancel_message(rf->b_session, rf->b_msg_session);
#else
			soup_session_cancel_message(rf->b_session, rf->b_msg_session, SOUP_STATUS_CANCELLED);
#endif
		}
		soup_session_abort(rf->b_session);
		rf->b_session = NULL;
		rf->b_msg_session = NULL;
	}
	rf->cancel_all = 0;
}

static void
readrss_dialog_cb (GtkWidget *widget, gpointer data)
{
#ifdef RSS_DEBUG
	g_print("\nCancel reading feeds\n");
#endif
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


/*        if (account->id->address)
                xmlNewTextChild (id, NULL, "addr-spec", account->id->address);
        if (account->id->reply_to)
                xmlNewTextChild (id, NULL, "reply-to", account->id->reply_to);
        if (account->id->organization)
                xmlNewTextChild (id, NULL, "organization", account->id->organization);

        node = xmlNewChild (id, NULL, "signature",NULL);
        xmlSetProp (node, "uid", key);

        src = xmlNewChild (root, NULL, "source", NULL);
        xmlSetProp (src, "save-passwd", account->source->save_passwd ? "true" : "false");
        xmlSetProp (src, "keep-on-server", account->source->keep_on_server ? "true" : "false");
        xmlSetProp (src, "auto-check", account->source->auto_check ? "true" : "false");
        sprintf (buf, "%d", account->source->auto_check_time);
        xmlSetProp (src, "auto-check-timeout", buf);
        if (account->source->url)
                xmlNewTextChild (src, NULL, "url", account->source->url);

        xport = xmlNewChild (root, NULL, "transport", NULL);
        xmlSetProp (xport, "save-passwd", account->transport->save_passwd ? "true" : "false");
        if (account->transport->url)
                xmlNewTextChild (xport, NULL, "url", account->transport->url);

        xmlNewTextChild (root, NULL, "drafts-folder", account->drafts_folder_uri);
        xmlNewTextChild (root, NULL, "sent-folder", account->sent_folder_uri);*/

	
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

#ifdef RSS_DEBUG
	g_print("checking hay:%s fro neddle:%s\n", sf_href, int_uri);
#endif

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

static void
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
}

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
        gboolean bool;
        char *buf;

        if ((buf = xmlGetProp (node, name))) {
                bool = (!strcmp (buf, "true") || !strcmp (buf, "yes"));
                xmlFree (buf);

                if (bool != *val) {
                        *val = bool;
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
	gchar *feed_dir = g_strdup_printf("%s/mail/rss", 
	    mail_component_peek_base_directory (mail_component_peek ()));
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

	if (g_file_test(feed_file, G_FILE_TEST_EXISTS))
		migrate_old_config(feed_file);
	else
		load_gconf_feed();

	res = 1;
out:	g_free(feed_file);
	return res;
}

void
html_set_base(xmlNode *doc, char *base, char *tag, char *prop, char *basehref)
{
	gchar *url;
#if LIBSOUP_VERSION < 2003000
	SoupUri *newuri;
#else
	SoupURI *newuri;
#endif
        gchar *newuristr;
#if LIBSOUP_VERSION < 2003000
        SoupUri *base_uri = soup_uri_new (base);
#else
        SoupURI *base_uri = soup_uri_new (base);
#endif
	while (doc = html_find((xmlNode *)doc, tag))
	{
		if (url = xmlGetProp(doc, prop))
		{
			if (!strncmp(tag, "img", 3) && !strncmp(prop, "src", 3))
			{
				gchar *tmpurl = strplchr(url);
				xmlSetProp(doc, prop, tmpurl);
				g_free(tmpurl);
			}
#ifdef RSS_DEBUG
			g_print("DEBUG: parsing: %s\n", url);
#endif
			if (url[0] == '/' && url[1] != '/')
			{
				gchar *server = get_server_from_uri(base);
				gchar *tmp = g_strdup_printf("%s/%s", server, url);
				xmlSetProp(doc, prop, tmp);
				g_free(tmp);
				g_free(server);
			}
			if (url[0] == '/' && url[1] == '/')
			{
				/*FIXME handle ssl */
				gchar *tmp = g_strdup_printf("%s%s", "http:", url);
				xmlSetProp(doc, prop, tmp);
				g_free(tmp);
			}
			if (url[0] != '/' && !g_str_has_prefix(url,  "http://") 
					&& !g_str_has_prefix(url, "https://"))
			{
				// in case we have a base href= set then rewrite
				// all relative links
				if (basehref != NULL)
				{
#if LIBSOUP_VERSION < 2003000
        				SoupUri *newbase_uri = soup_uri_new (basehref);
#else
        				SoupURI *newbase_uri = soup_uri_new (basehref);
#endif
        				newuri = soup_uri_new_with_base (newbase_uri, url);
					soup_uri_free(newbase_uri);
				}
				else	
        				newuri = soup_uri_new_with_base (base_uri, url);
				//xmlSetProp(doc, prop, g_strdup_printf("%s/%s", get_server_from_uri(base), url));
				if (newuri)
				{
        				newuristr = soup_uri_to_string (newuri, FALSE);
					xmlSetProp(doc, prop, (xmlChar *)newuristr);
					g_free(newuristr);
					soup_uri_free(newuri);
				}
			}
			xmlFree(url);
		}
	}
	soup_uri_free(base_uri);
}

static void
my_xml_parser_error_handler (void *ctx, const char *msg, ...)
{
        ;
}

xmlDoc *
xml_parse_sux (const char *buf, int len)
{
        static xmlSAXHandler *sax;
        xmlParserCtxtPtr ctxt;
        xmlDoc *doc;

        g_return_val_if_fail (buf != NULL, NULL);

        if (!sax) {
                xmlInitParser();
                sax = xmlMalloc (sizeof (xmlSAXHandler));
//#if LIBXML_VERSION > 20600 
                xmlSAXVersion (sax, 2);
//#else
  //              memcpy (sax, &xmlDefaultSAXHandler, sizeof (xmlSAXHandler));
//#endif
                sax->warning = my_xml_parser_error_handler;
                sax->error = my_xml_parser_error_handler;
        }

        if (len == -1)
                len = strlen (buf);
        ctxt = xmlCreateMemoryParserCtxt (buf, len);
        if (!ctxt)
                return NULL;

        xmlFree (ctxt->sax);
        ctxt->sax = sax;
//#if LIBXML_VERSION > 20600
        ctxt->sax2 = 1;
        ctxt->str_xml = xmlDictLookup (ctxt->dict, BAD_CAST "xml", 3);
        ctxt->str_xmlns = xmlDictLookup (ctxt->dict, BAD_CAST "xmlns", 5);
        ctxt->str_xml_ns = xmlDictLookup (ctxt->dict, XML_XML_NAMESPACE, 36);
//#endif

        ctxt->recovery = TRUE;
        ctxt->vctxt.error = my_xml_parser_error_handler;
        ctxt->vctxt.warning = my_xml_parser_error_handler;

	xmlCtxtUseOptions(ctxt, XML_PARSE_DTDLOAD
				| XML_PARSE_NOENT
				| XML_PARSE_NOCDATA);

        xmlParseDocument (ctxt);

        doc = ctxt->myDoc;
        ctxt->sax = NULL;
        xmlFreeParserCtxt (ctxt);

        return doc;
}

xmlDoc *
parse_html_sux (const char *buf, int len)
{
        xmlDoc *doc;
#if LIBXML_VERSION > 20600
        static xmlSAXHandler *sax;
        htmlParserCtxtPtr ctxt;

        g_return_val_if_fail (buf != NULL, NULL);

        if (!sax) {
                xmlInitParser();
                sax = xmlMalloc (sizeof (htmlSAXHandler));
                memcpy (sax, &htmlDefaultSAXHandler, sizeof (xmlSAXHandlerV1));
                sax->warning = my_xml_parser_error_handler;
                sax->error = my_xml_parser_error_handler;
        }

        if (len == -1)
                len = strlen (buf);
        ctxt = htmlCreateMemoryParserCtxt (buf, len);
        if (!ctxt)
                return NULL;

        xmlFree (ctxt->sax);
        ctxt->sax = sax;
        ctxt->vctxt.error = my_xml_parser_error_handler;
        ctxt->vctxt.warning = my_xml_parser_error_handler;

        htmlParseDocument (ctxt);
        doc = ctxt->myDoc;

        ctxt->sax = NULL;
        htmlFreeParserCtxt (ctxt);

#else /* LIBXML_VERSION <= 20600 */
        char *buf_copy = g_strndup (buf, len);

        doc = htmlParseDoc (buf_copy, NULL);
        g_free (buf_copy);
#endif
        return doc;
}

/*modifies a html document to be absolute */
xmlDoc *
parse_html(char *url, const char *html, int len)
{
	xmlDoc *src = NULL;
	xmlDoc *doc = NULL;

	src = (xmlDoc *)parse_html_sux(html, len);

	if (!src)
		return NULL;
	doc = src;
	gchar *newbase = NULL;
	newbase = xmlGetProp(html_find((xmlNode *)doc, "base"), "href");
#ifdef RSS_DEBUG
	g_print("newbase:|%s|\n", newbase);
#endif
	xmlDoc *tmpdoc = (xmlDoc *)html_find((xmlNode *)doc, "base");
	xmlUnlinkNode((xmlNode *)tmpdoc);
	html_set_base((xmlNode *)doc, url, "a", "href", newbase);
	html_set_base((xmlNode *)doc, url, "img", "src", newbase);
	html_set_base((xmlNode *)doc, url, "input", "src", newbase);
	html_set_base((xmlNode *)doc, url, "link", "src", newbase);
	html_set_base((xmlNode *)doc, url, "body", "background", newbase);
	html_set_base((xmlNode *)doc, url, "script", "src", newbase);
/*	while (doc = html_find((xmlNode *)doc, "img"))
	{
		if (url = xmlGetProp(doc, "src"))
		{
			gchar *str = strplchr(url);
                	g_print("%s\n", str);
                	xmlSetProp(doc, "src", str);
			g_free(str);
			xmlFree(url);
                }
	}*/
	doc = src;
	if (newbase)
		xmlFree(newbase);
	return doc;
}

static gchar *
parse_href (const gchar *s, const gchar *base)
{
        gchar *retval;
        gchar *tmp;
        gchar *tmpurl;

        if(s == NULL || *s == 0)
                return g_strdup ("");

//	tmpurl = html_url_new (s);
//        if (html_url_get_protocol (tmpurl) == NULL) {
                if (s[0] == '/') {
                        if (s[1] == '/') {
                                gchar *t;

                                /* Double slash at the beginning.  */

                                /* FIXME?  This is a bit sucky.  */
/*                                t = g_strconcat (html_url_get_protocol (baseURL),
                                                 ":", s, NULL);
                                html_url_destroy (tmpurl);
                                tmpurl = html_url_new (t);
                                retval = html_url_to_string (tmpurl);
                                html_url_destroy (tmpurl);
                                g_free (t);*/
                        } else {
                                /* Single slash at the beginning.  */

				tmpurl = g_strdup_printf("%s%s", base, s);
                        }
                } else {
                                gchar *t;
/*                        html_url_destroy (tmpurl);
                        tmpurl = html_url_append_path (baseURL, s);
                        retval = html_url_to_string (tmpurl);
                        html_url_destroy (tmpurl);*/
                }
//        } else {
  //              retval = html_url_to_string (tmpurl);
    //            html_url_destroy (tmpurl);
      //  }

        return tmpurl;
}

static void
summary_cb (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
	rf->cur_format = rf->cur_format^1;
	rf->chg_format = 1;
	em_format_redraw((EMFormat *)pobject);
	while (gtk_events_pending ())
             gtk_main_iteration ();
	
}

static void
stop_cb (GtkWidget *button, EMFormatHTMLPObject *pobject)
{
#ifdef	HAVE_GTKMOZEMBED
	gtk_moz_embed_stop_load(GTK_MOZ_EMBED(rf->mozembed));
#endif
#ifdef	HAVE_WEBKIT
	webkit_gtk_page_stop_loading(WEBKIT_GTK_PAGE(rf->mozembed));
#endif
}

reload_cb (GtkWidget *button, gpointer data)
{
	guint engine = gconf_client_get_int(rss_gconf, GCONF_KEY_HTML_RENDER, NULL);
	switch (engine)
	{
		case 2:
#ifdef	HAVE_GTKMOZEMBED
	gtk_moz_embed_stop_load(GTK_MOZ_EMBED(rf->mozembed));
       	gtk_moz_embed_load_url (GTK_MOZ_EMBED(rf->mozembed), data);
#endif
		break;
		case 1:
#ifdef	HAVE_WEBKIT
	webkit_gtk_page_stop_loading(WEBKIT_GTK_PAGE(rf->mozembed));
     	webkit_gtk_page_open(WEBKIT_GTK_PAGE(rf->mozembed), data);
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
  //      EMFormatHTMLDisplay *efhd = (EMFormatHTMLDisplay *) efh;
  	EMFormatHTML *efh = data;

//        gtk_widget_size_request (efhd->priv->attachment_bar, &req);
//        gtk_widget_size_request (gtk_widget_get_parent((GtkWidget *)efh->html), &req);
//	g_print("BOX w:%d,h:%d\n", req.width, req.height);
  //      width = ((GtkWidget *) efh->html)->allocation.height - 16;
//	g_print("WID:%d\n", width);
	
	guint k = rf->headers_mode ? 198 : 103;
	if (GTK_IS_WIDGET(widget))
	{
        	width = widget->allocation.width - 16 - 2;// - 16;
        	int height = widget->allocation.height - 16 - k;
		g_print("resize webkit :width:%d, height: %d\n", width, height);
#ifdef RSS_DEBUG
		g_print("resize webkit :width:%d, height: %d\n", width, height);
#endif
//			rf->headers_mode ? 194 : 100;
//	EMFormat *myf = (EMFormat *)efh;
//	g_print("w0:%d,h0:%d\n", width, height);
//	GtkRequisition req;
	//get eb
//	gtk_widget_size_request(data, &req);
//	GtkWidget *my = data;
//	g_print("w:%d,h:%d\n", req.width, req.height);
//	g_print("w2:%d,h2:%d\n", my->allocation.width, my->allocation.height);
//	int wheight = height - (req.height - height) - 20;
//	g_print("size:%d\n", wheight);
//        height = req.height - 200;// - 16 - 194;
//	g_print("my cal %d w:%d h:%d\n", GTK_IS_WIDGET(data), width, height);
/*		g_print("data:%p\n", data);
		g_print("is_widget:%d\n", GTK_IS_WIDGET(widget));
		g_print("is_data:%p\n", data);
		g_print("is_is_data:%d\n", GTK_IS_WIDGET(gtk_bin_get_child(data)));
		g_print("is_is_data:%d\n", GTK_IS_WIDGET(data));*/
		if (data)
			if(GTK_IS_WIDGET(data) && height > 50)
			{
				gtk_widget_set_size_request((GtkWidget *)data, width, height);
// apparently resizing gtkmozembed widget won't redraw if using xulrunner
// there is no point in reload for the rest
#ifdef HAVE_XULRUNNER
				gtk_moz_embed_reload(rf->mozembed, GTK_MOZ_EMBED_FLAG_RELOADNORMAL);
#endif
			}
	}
	g_print("resize done\n");
}

#ifdef HAVE_GTKMOZEMBED
void
rss_mozilla_init(void)
{
	GError *err = NULL;
       	g_setenv("MOZILLA_FIVE_HOME", GECKO_HOME, 1);
	g_unsetenv("MOZILLA_FIVE_HOME");

	gtk_moz_embed_set_comp_path(GECKO_HOME);
	gchar *profile_dir = g_build_filename (g_get_home_dir (),
                                              ".evolution",
                                              "mail",
                                              "rss", NULL);

        gtk_moz_embed_set_profile_path (profile_dir, "mozembed-rss");
        g_free (profile_dir);
	if (!g_thread_supported ()) {
               	g_thread_init (NULL);
       	}
	gtk_moz_embed_push_startup ();
}
#endif

#ifdef HAVE_RENDERKIT
static gboolean
org_gnome_rss_controls2 (EMFormatHTML *efh, void *eb, EMFormatHTMLPObject *pobject)
{
	struct _org_gnome_rss_controls_pobject *po = (struct _org_gnome_rss_controls_pobject *) pobject;
	int width, height;
        GtkRequisition req;
	GtkWidget *moz;

//        gtk_widget_size_request (efhd->priv->attachment_bar, &req);
	guint engine = gconf_client_get_int(rss_gconf, GCONF_KEY_HTML_RENDER, NULL);
	moz = gtk_scrolled_window_new(NULL,NULL);
//	moz = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(moz),
                                       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	

#ifdef HAVE_WEBKIT
	if (engine == 1)
	{
//		if (!g_thread_supported ()) {
  //              	g_thread_init (NULL);
    //    	}
		webkit_gtk_init();
		rf->mozembed = (GtkWidget *)webkit_gtk_page_new();
		gtk_container_add(GTK_CONTAINER(moz), GTK_WIDGET(rf->mozembed));
	}
#endif

#ifdef HAVE_GTKMOZEMBED
	if (engine == 2)
	{
		if (!g_thread_supported ()) {
                	g_thread_init (NULL);
			gdk_threads_init();
        	}

/*		if (!rf->test && rf->test < 2)
		{
			gtk_moz_embed_push_startup ();
			rf->test++;
		}*/

		rf->mozembed = gtk_moz_embed_new();

		/* FIXME add all those profile shits */
		gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(moz), GTK_WIDGET(rf->mozembed));
//		gtk_container_add(GTK_CONTAINER(moz), GTK_WIDGET(rf->mozembed));
//		gtk_box_pack_start(moz, rf->mozembed, FALSE, FALSE, 0);
	}
#endif

//	po->html = rf->mozembed;
	po->container = moz;

#ifdef HAVE_WEBKIT
	if (engine == 1)
	{
		g_print("Render engine Webkit\n");
		if (rf->online)
        		webkit_gtk_page_open(WEBKIT_GTK_PAGE(rf->mozembed), po->website);
		else
        		webkit_gtk_page_open(WEBKIT_GTK_PAGE(rf->mozembed), "about:blank");
	}
#endif

#ifdef HAVE_GTKMOZEMBED
	if (engine == 2)
	{
		g_print("Render engine Gecko\n");
		if (rf->online)
		{
			gtk_moz_embed_stop_load(GTK_MOZ_EMBED(rf->mozembed));
        		gtk_moz_embed_load_url (GTK_MOZ_EMBED(rf->mozembed), po->website);
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
	po->shandler = g_signal_connect(efh->html,
		"size_allocate",
		G_CALLBACK(mycall),
		moz);
	return TRUE;
}
#endif


static gboolean
org_gnome_rss_controls (EMFormatHTML *efh, void *eb, EMFormatHTMLPObject *pobject)
{
	struct _org_gnome_rss_controls_pobject *po = (struct _org_gnome_rss_controls_pobject *) pobject;
	GtkWidget *vbox = gtk_vbox_new (TRUE, 1);
	gtk_widget_show (vbox);
	GtkWidget *hbox2 = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox2);

	GtkWidget *label3 = gtk_label_new ("");
	gchar *mem = g_strdup_printf(" <b>%s:</b>", _("Feed view"));
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label3), mem);
	gtk_widget_show (label3);
	gtk_box_pack_start (GTK_BOX (hbox2), label3, TRUE, TRUE, 0);
	gtk_widget_set_size_request (GTK_WIDGET(hbox2), -1, 31);

	GtkWidget *button = gtk_button_new_with_label(
				rf->cur_format ? _("HTML") : _("Summary"));
	gtk_button_set_image (
                GTK_BUTTON (button),
                gtk_image_new_from_stock (
                        GTK_STOCK_HOME, GTK_ICON_SIZE_BUTTON));
	g_signal_connect (button, "clicked", G_CALLBACK(summary_cb), efh);
	gtk_widget_set_size_request(button, 100, 10);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_HALF);
        gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox2), button, TRUE, TRUE, 0);
	if (rf->cur_format)
	{
        	GtkWidget *button2 = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
		g_signal_connect (button2, "clicked", G_CALLBACK(stop_cb), efh);
		gtk_widget_set_size_request(button2, 100, 10);
		gtk_button_set_relief(GTK_BUTTON(button2), GTK_RELIEF_HALF);
		gtk_widget_set_sensitive (button2, rf->online);
        	gtk_widget_show (button2);
		gtk_box_pack_start (GTK_BOX (hbox2), button2, TRUE, TRUE, 0);
        	GtkWidget *button3 = gtk_button_new_from_stock (GTK_STOCK_REFRESH);
		g_signal_connect (button3, "clicked", G_CALLBACK(reload_cb), po->website);
		gtk_widget_set_size_request(button3, 100, -1);
		gtk_button_set_relief(GTK_BUTTON(button3), GTK_RELIEF_HALF);
		gtk_widget_set_sensitive (button3, rf->online);
        	gtk_widget_show (button3);
		gtk_box_pack_start (GTK_BOX (hbox2), button3, TRUE, TRUE, 0);
	}
	gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0);

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
#ifdef HAVE_GTKMOZEMBED
	if (engine == 2)
	{
		gtk_moz_embed_stop_load(GTK_MOZ_EMBED(rf->mozembed));
//		gtk_moz_embed_pop_startup();
	}
#endif
	g_signal_handler_disconnect(po->format->html, po->shandler);
	if (rf->mozembed)
	{
		g_print("call pfree() for controls2\n");
		gtk_widget_destroy(rf->mozembed);
		rf->mozembed = NULL;
	}
	gtk_widget_destroy(po->container);
	g_free(po->website);
}

void org_gnome_cooly_format_rss(void *ep, EMFormatHookTarget *t);

void org_gnome_cooly_format_rss(void *ep, EMFormatHookTarget *t)	//camelmimepart
{
        GError *err = NULL;
        GString *content;
	xmlChar *buff = NULL;
	int size = 0;
	unsigned char *buffer2 = NULL;
   	int inlen, utf8len;
	CamelDataWrapper *dw = camel_data_wrapper_new();
	CamelMimePart *part = camel_mime_part_new();
	CamelStream *fstream = camel_stream_mem_new();
#ifdef RSS_DEBUG
        g_print("Formatting...\n");
#endif

	CamelMimePart *message = CAMEL_IS_MIME_MESSAGE(t->part) ? 
			t->part : 
			(CamelMimePart *)t->format->message;
	const char *website = camel_medium_get_header (CAMEL_MEDIUM (message), "Website");
	if (!website)
		goto fmerror;
	gchar *addr = (gchar *)camel_header_location_decode(website);
	gchar *feedid = NULL;
	feedid  = (gchar *)camel_medium_get_header (CAMEL_MEDIUM(message), "RSS-ID");
	gchar *subject = camel_header_decode_string(camel_medium_get_header (CAMEL_MEDIUM (message),
				 "Subject"), NULL);
	
	
	gpointer is_html = NULL;
	if (feedid)
		is_html =  g_hash_table_lookup(rf->hrh, g_strstrip(feedid)); 
	
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

	EMFormatHTML *emfh = (EMFormatHTML *)t->format;
	/* force loading of images even if mail images disabled */
	emfh->load_http_now = TRUE;

	if (rf->cur_format || (feedid && is_html && rf->cur_format))
	{
#ifdef HAVE_RENDERKIT
	guint engine = gconf_client_get_int(rss_gconf, GCONF_KEY_HTML_RENDER, NULL);
#if !defined(HAVE_GTKMOZEMBED) && !defined (HAVE_WEBKIT)
	engine=0;
#endif

	if (engine && engine != 10)
	{ 
        	char *classid = g_strdup_printf ("org-gnome-rss-controls-%d",
			org_gnome_rss_controls_counter_id);
		org_gnome_rss_controls_counter_id++;
		pobj = (struct _org_gnome_rss_controls_pobject *) em_format_html_add_pobject ((EMFormatHTML *) t->format, sizeof(*pobj), classid, message, (EMFormatHTMLPObjectFunc)org_gnome_rss_controls2);
		pobj->website = g_strstrip(g_strdup((gchar *)website));
		pobj->is_html = GPOINTER_TO_INT(is_html);
		pobj->format = (EMFormatHTML *)t->format;
		pobj->object.free = pfree;
        	camel_stream_printf (t->stream, "<table><tr><td width=100%% valign=top><object classid=%s></object></td></tr></table>\n", classid);
		goto out;
	}
#endif
		content = net_post_blocking(addr, NULL, NULL, textcb, NULL, &err);
		if (err)
        	{
			//we do not need to setup a pop error menu since we're in 
			//formatting process. But instead display mail body an error
			//such proxy error or transport error
			camel_stream_printf (t->stream, "<table border=1 width=\"100%%\" cellpadding=0 cellspacing=0><tr><td bgcolor=#ffffff>");
			camel_stream_printf(t->stream, "<table border=0 width=\"100%%\" cellspacing=4 cellpadding=4><tr>");
     			camel_stream_printf (t->stream, "<td bgcolor=\"#ffffff\">%s</td>", err->message);
    			camel_stream_printf (t->stream, "</tr></table></td></tr></table>");
                	goto out;
        	}

		inlen = content->len;
		utf8len = 5*inlen+1;
		buffer2 = g_malloc(utf8len);
		UTF8ToHtml(buffer2, &utf8len, content->str, &inlen);
		xmlDoc *src = (xmlDoc *)parse_html(addr, buffer2, strlen(buffer2));

		if (src)
		{
			htmlDocDumpMemory(src, &buff, &size);
#ifdef RSS_DEBUG
			g_print("%s\n", buff);
#endif
			g_print("%s\n", buff);
			xmlFree(src);
		}
		else
			goto out;

		camel_stream_printf(fstream,
		 "<table border=1 width=\"100%%\" cellpadding=0 cellspacing=0><tr><td bgcolor=#ffffff>");
		camel_stream_printf(fstream,
		 "<table border=0 width=\"100%%\" cellspacing=4 cellpadding=4>");
   		camel_stream_printf(fstream,
		 "<tr><td bgcolor=\"#ffffff\"><b><font size=+1><a href=%s>%s</a></font></b></td></tr>", website, subject);
     		camel_stream_printf(fstream, "</head></html><tr><td bgcolor=\"#ffffff\">%s</td>", buff);
    		camel_stream_printf(fstream, "</tr></table></td></tr></table>");
		if (buff)
			g_free(buff);
		g_free(subject);
		g_string_free(content, 1);
	}
	else
	{
#ifdef RSS_DEBUG
		g_print("normal html rendering\n");
#endif
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
		inlen = buffer->len;
		utf8len = 5*inlen+1;
		buffer2 = g_malloc(utf8len);
		UTF8ToHtml(buffer2, &utf8len, buffer->data, &inlen);
		g_byte_array_free (buffer, 1);
		xmlDoc *src = (xmlDoc *)parse_html_sux(buffer2, strlen(buffer2));
		if (src)
		{
			xmlNode *doc = (xmlNode *)src;

			while (doc = html_find(doc, "img"))
        		{
                		xmlChar *url = xmlGetProp(doc, "src");
				if (url)
				{
					gchar *str = strplchr((gchar *)url);
					xmlFree(url);
					xmlSetProp(doc, "src", str);
					g_free(str);
				}
			}
			xmlDocDumpMemory(src, &buff, &size);
		}
		else goto out;	
		char *tmp = decode_html_entities(buff);
		g_free(buff);
		buff = tmp;

//#endif
#ifdef RSS_DEBUG
		g_print("%s\n", buff);
#endif
		camel_stream_printf (fstream, 
		"<table border=1 width=\"100%%\" cellpadding=0 cellspacing=0><tr><td bgcolor=#ffffff>");
		camel_stream_printf(fstream, 
		"<table border=0 width=\"100%%\" cellspacing=4 cellpadding=4><tr>");
     		camel_stream_printf(fstream,
		 "<tr><td bgcolor=\"#ffffff\"><b><font size=+1><a href=%s>%s</a></font></b></td></tr>", website, subject);
     		camel_stream_printf (fstream, "<td bgcolor=\"#ffffff\">%s</td>", buff);
    		camel_stream_printf (fstream, "</tr></table></td></tr></table>");
	}

	//this is required for proper charset rendering when html
       	camel_data_wrapper_construct_from_stream(dw, fstream);
       	camel_medium_set_content_object((CamelMedium *)part, dw);
	em_format_format_text((EMFormat *)t->format, (CamelStream *)t->stream, (CamelDataWrapper *)part);
	camel_object_unref(dw);
	camel_object_unref(part);
	camel_object_unref(fstream);

out:	if (addr)
		g_free(addr);
	if (buffer2)
		g_free(buffer2);
	return;
fmerror:
	camel_stream_printf (t->stream, 
	"<table border=1 width=\"100%%\" cellpadding=0 cellspacing=0><tr><td bgcolor=#ffffff>");
	camel_stream_printf(t->stream, 
	"<table border=0 width=\"100%%\" cellspacing=4 cellpadding=4><tr>");
     	camel_stream_printf (t->stream,
	"<td bgcolor=\"#ffffff\">Cannot format email. Formatting error!</td>");
    	camel_stream_printf (t->stream, "</tr></table></td></tr></table>");
	return;
}

#ifdef EVOLUTION_2_12
void org_gnome_cooly_article_show(void *ep, EMEventTargetMessage *t);
#else
void org_gnome_cooly_article_show(void *ep, void *t);
#endif

#ifdef EVOLUTION_2_12
void org_gnome_cooly_article_show(void *ep, EMEventTargetMessage *t)
{
	if (rf)
		rf->current_uid = t->uid;
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

gboolean
setup_feed(add_feed *feed)
{
	CamelException ex;
	guint ret = 0;
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

	rf->pending = TRUE;

	if (!feed->validate)
		goto add;
		
	g_print("feed->feed_url:%s\n", feed->feed_url);
        content = net_post_blocking(feed->feed_url, NULL, post, textcb, rf, &err);
        if (err)
	{
		g_print("err:%s\n", err->message);
		rss_error(NULL, feed->feed_name ? feed->feed_name: "Unamed feed", _("Error while fetching feed."), err->message);
		goto out;
        }
        xmlDocPtr doc = NULL;
        xmlNodePtr root = NULL;
        xmlSubstituteEntitiesDefaultValue = 0;
        doc = xml_parse_sux (content->str, content->len);
#ifdef RSS_DEBUG
	g_print("content:%s\n", content->str);
#endif
	root = xmlDocGetRootElement(doc);

	if ((doc != NULL && root != NULL)
		&& (strcasestr(root->name, "rss")
		|| strcasestr(root->name, "rdf")
		|| strcasestr(root->name, "feed")))
	{
        	r->cache = doc;
		r->uri = feed->feed_url;

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
	}
	else
	{
		rss_error(NULL, NULL, _("Error while fetching feed."), _("Invalid Feed"));
		ret = 0;
	}
out:	rf->pending = FALSE;
	return ret;
}

void
#if LIBSOUP_VERSION < 2003000
finish_feed (SoupMessage *msg, gpointer user_data)
#else
finish_feed (SoupSession *soup_sess, SoupMessage *msg, gpointer user_data)
#endif
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

	if (rf->feed_queue)
		rf->feed_queue--;


#ifndef EVOLUTION_2_12
	if(rf->progress_dialog && rf->feed_queue == 0)
        {
              gtk_widget_destroy(rf->progress_dialog);
              rf->progress_dialog = NULL;
              rf->progress_bar = NULL;
        }
#else
	if(rf->label && rf->feed_queue == 0 && rf->info)
	{
                        gtk_label_set_markup (GTK_LABEL (rf->label), _("Canceled"));
                if (rf->info->cancel_button)
                        gtk_widget_set_sensitive(rf->info->cancel_button, FALSE);

                g_hash_table_remove(rf->info->data->active, rf->info->uri);
//                rf->info->data->infos = g_list_remove(rf->info->data->infos, rf->info);

                if (g_hash_table_size(rf->info->data->active) == 0) {
                        if (rf->info->data->gd)
                                gtk_widget_destroy((GtkWidget *)rf->info->data->gd);
                }
                //clean data that might hang on rf struct
                rf->sr_feed = NULL;
                rf->label = NULL;
                rf->progress_bar = NULL;
                rf->info = NULL;
	}
#endif

	if (rf->cancel_all)
		goto out;

	if (msg->status_code != SOUP_STATUS_OK &&
	    msg->status_code != SOUP_STATUS_CANCELLED) {
        	g_set_error(&err, NET_ERROR, NET_ERROR_GENERIC,
                	soup_status_get_phrase(msg->status_code));
                gchar *msg = g_strdup_printf("\n%s\n%s", user_data, err->message);
                rss_error(user_data, NULL, _("Error fetching feed."), msg);
                g_free(msg);
        	goto out;
    	}

	if (rf->cancel)
	{
#ifdef EVOLUTION_2_12
		if(rf->label && rf->feed_queue == 0 && rf->info)
        	{
                	gtk_label_set_markup (GTK_LABEL (rf->label), _("Canceled"));
                if (rf->info->cancel_button)
                        gtk_widget_set_sensitive(rf->info->cancel_button, FALSE);

                g_hash_table_remove(rf->info->data->active, rf->info->uri);
                rf->info->data->infos = g_list_remove(rf->info->data->infos, rf->info);

                if (g_hash_table_size(rf->info->data->active) == 0) {
                        if (rf->info->data->gd)
                                gtk_widget_destroy((GtkWidget *)rf->info->data->gd);
                }
                //clean data that might hang on rf struct
                rf->sr_feed = NULL;
                rf->label = NULL;
                rf->progress_bar = NULL;
                rf->info = NULL;
		}
#endif
		goto out;
	}
	
#if LIBSOUP_VERSION < 2003000
	if (!msg->response.length)
#else
	if (!msg->response_body->length)
#endif
		goto out;

	if (msg->status_code == SOUP_STATUS_CANCELLED)
		goto out;


#if LIBSOUP_VERSION < 2003000
	GString *response = g_string_new_len(msg->response.body, msg->response.length);
#else
	GString *response = g_string_new_len(msg->response_body->data, msg->response_body->length);
#endif
//#ifdef RSS_DEBUG
	g_print("feed %s\n", user_data);
//#endif
//
	while (gtk_events_pending ())
            gtk_main_iteration ();

	RDF *r = g_new0 (RDF, 1);
        r->shown = TRUE;
        xmlSubstituteEntitiesDefaultValue = 1;
        r->cache = xml_parse_sux (response->str, response->len);

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
			}
			g_free(chn_name);
		}
		if (r->cache)
			xmlFreeDoc(r->cache);
		if (r->type)
			g_free(r->type);
		if (r->version)
			g_free(r->version);
	}
	g_free(r);
	g_string_free(response, 1);

	if (!deleted)
		if (g_hash_table_lookup(rf->hrdel_feed, lookup_key(user_data)))
			get_feed_age(user_data, lookup_key(user_data));
//tout:	

#ifdef EVOLUTION_2_12
	if (rf->sr_feed && !deleted)
	{
		gchar *furl;
		gchar *type = g_hash_table_lookup(rf->hrt, lookup_key(user_data));
		if (strncmp(type, "-",1) == 0)
			furl = g_strdup_printf("<b>%s</b>: %s", 
					"RSS", user_data);
		else
			furl = g_strdup_printf("<b>%s</b>: %s", 
			type, user_data);
		gtk_label_set_markup (GTK_LABEL (rf->sr_feed), furl);
		g_free(furl);
	}
	if(rf->label && rf->feed_queue == 0 && rf->info)
	{
		gtk_label_set_markup (GTK_LABEL (rf->label), _("Complete"));
        	if (rf->info->cancel_button)
                	gtk_widget_set_sensitive(rf->info->cancel_button, FALSE);

        	g_hash_table_remove(rf->info->data->active, rf->info->uri);
        	rf->info->data->infos = g_list_remove(rf->info->data->infos, rf->info);

        	if (g_hash_table_size(rf->info->data->active) == 0) {
                	if (rf->info->data->gd)
                        	gtk_widget_destroy((GtkWidget *)rf->info->data->gd);
        	}
		//clean data that might hang on rf struct
		rf->sr_feed = NULL;
		rf->label = NULL;
		rf->progress_bar = NULL;
		rf->info = NULL;
	}
#endif
out:	
	if (user_data)
	{
		taskbar_op_finish(user_data);
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
//	rf->cfeed = key;

	if (!rf->activity)
		rf->activity = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	if (!rf->error_hash)
		rf->error_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	// check if we're enabled and no cancelation signal pending
	// and no imports pending
	if (g_hash_table_lookup(rf->hre, lookup_key(key)) && !rf->cancel && !rf->import)
	{
#ifdef RSS_DEBUG
		g_print("\nFetching: %s..%s\n", g_hash_table_lookup(rf->hr, lookup_key(key)), key);
#endif
		rf->feed_queue++;

		gchar *tmsg;
		gchar *type = g_hash_table_lookup(rf->hrt, lookup_key(key));
        	if (strncmp(type, "-",1) == 0)
                        tmsg = g_strdup_printf("Fetching %s: %s", 
                                        "RSS", key);
        	else
                        tmsg = g_strdup_printf("Fetching %s: %s", 
                        type, key);

#if (EVOLUTION_VERSION >= 22200)
		guint activity_id = taskbar_op_new(tmsg, key);
#else
		guint activity_id = taskbar_op_new(tmsg);
#endif

		g_free(tmsg);
		g_hash_table_insert(rf->activity, key, GUINT_TO_POINTER(activity_id));
		net_get_unblocking(
				g_hash_table_lookup(rf->hr, lookup_key(key)),
				user_data,
				key,
				(gpointer)finish_feed,
				g_strdup(key),	// we need to dupe key here
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

gboolean
update_articles(gboolean disabler)
{
	if (!rf->pending && !rf->feed_queue && rf->online)
	{
		g_print("Reading RSS articles...\n");
		rf->pending = TRUE;
		check_folders();
		rf->err = NULL;
		g_hash_table_foreach(rf->hrname, fetch_feed, statuscb);	
		rf->pending = FALSE;
	}
	return disabler;
}

gchar *
get_main_folder(void)
{
	gchar mf[512];
	gchar *feed_dir = g_strdup_printf("%s/mail/rss",
            mail_component_peek_base_directory (mail_component_peek ()));
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
	gchar *feed_dir = g_strdup_printf("%s/mail/rss",
            mail_component_peek_base_directory (mail_component_peek ()));
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
update_main_folder(gchar *new_name)
{
	FILE *f;
	if (rf->main_folder)
		g_free(rf->main_folder);
	rf->main_folder = g_strdup(new_name);
	
	gchar *feed_dir = g_strdup_printf("%s/mail/rss",
            mail_component_peek_base_directory (mail_component_peek ()));
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
	gchar *oname = g_path_get_basename(old_name);
	gchar *nname = g_path_get_basename(new_name);
	FILE *f;
	gchar *feed_dir = g_strdup_printf("%s/mail/rss",
            mail_component_peek_base_directory (mail_component_peek ()));
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
store_folder_renamed(CamelObject *o, void *event_data, void *data)
{
	CamelRenameInfo *info = event_data;

	printf("Folder renamed to '%s' from '%s'\n", info->new->full_name, info->old_base);

	gchar *main_folder = lookup_main_folder();

	g_print("main_folder:%s\n", main_folder);

	if (!g_ascii_strncasecmp(main_folder, info->old_base, strlen(info->old_base)))
		update_main_folder(info->new->full_name);
	else
		update_feed_folder(info->old_base, info->new->full_name);
}

static void
rss_online(CamelObject *o, void *event_data, void *data)
{
	rf->online =  camel_session_is_online (o);
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
        
        /* hook in rename event to catch feeds folder rename */
	CamelStore *store = mail_component_peek_local_store(NULL);
	camel_object_hook_event(store, "folder_renamed",
                                (CamelObjectEventHookFunc)store_folder_renamed, NULL);
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

        if (!rf->setup || g_hash_table_size(rf->hrname)<1)
        {
                e_error_run(NULL,
			"org-gnome-evolution-rss:generr",
			_("No RSS feeds configured!"),
			NULL);
                return;
        }
	if (!feeds_enabled())
	{
                e_error_run(NULL,
			"org-gnome-evolution-rss:feederr",
			_("No RSS feeds enabled!"),
			_("Go to Edit->Preferences->News & Blogs to enable feeds."),
			NULL);
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
#if GTK_VERSION > 2006000
	gtk_label_set_ellipsize (GTK_LABEL (label2), PANGO_ELLIPSIZE_START);
#endif
#if GTK_VERSION < 2008000
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
                gtk_progress_bar_set_text((GtkProgressBar *)readrss_progress, _("0% done"));
                gtk_widget_show_all(readrss_dialog);
                rf->progress_dialog = readrss_dialog;
                rf->progress_bar = readrss_progress;
                rf->label       = label2;
        }
        if (!rf->pending && !rf->feed_queue)
        {
                rf->pending = TRUE;
                check_folders();

                rf->err = NULL;
                g_hash_table_foreach(rf->hrname, fetch_feed, statuscb);
                // reset cancelation signal
                if (rf->cancel)
                        rf->cancel = 0;
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
op_status(CamelOperation *op, const char *what, int pc, void *data)
{
        struct _send_info *info = data;

        //printf("Operation '%s', percent %d\n");
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

	rf->t = t;

	if (!rf->setup || g_hash_table_size(rf->hrname)<1)
	{
		e_error_run(NULL, "org-gnome-evolution-rss:generr", "No RSS feeds configured!", NULL);
		return;
	}

#ifdef EVOLUTION_2_12
	struct _send_info *info;
	struct _send_data *data = (struct _send_data *)t->data;

        info = g_malloc0 (sizeof (*info));
//        info->type = type;
                        
        info->uri = g_strdup("feed"); //g_stddup

        info->cancel = camel_operation_new (op_status, info);
        info->state = SEND_ACTIVE;
//        info->timeout_id = g_timeout_add (STATUS_TIMEOUT, operation_status_timeout, info);
                        
        g_hash_table_insert (data->active, info->uri, info);
//        list = g_list_prepend (list, info);

	gchar *iconfile = g_build_filename (EVOLUTION_ICONDIR,
	                                    "rss.png",
                                            NULL);

	GtkWidget *recv_icon = e_icon_factory_get_image (
                        iconfile, E_ICON_SIZE_LARGE_TOOLBAR);
	g_free(iconfile);


	guint row = t->row;
	row+=2;
	t->row = row;

	gtk_table_resize(GTK_TABLE(t->table), t->row, 4);

        char *pretty_url = g_strdup ("RSS");
        label = gtk_label_new (NULL);
#if GTK_VERSION > 2006000
        gtk_label_set_ellipsize (
                GTK_LABEL (label), PANGO_ELLIPSIZE_END);
#endif
#if GTK_VERSION < 2008000
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
    		gtk_progress_bar_set_text((GtkProgressBar *)readrss_progress, _("0% done"));
    		gtk_widget_show_all(readrss_dialog);
		rf->progress_dialog = readrss_dialog;
		rf->progress_bar = readrss_progress;
		rf->label	= label2;
	}
#endif
bail:	if (!rf->pending && !rf->feed_queue)
	{
		rf->pending = TRUE;
		check_folders();
	
		rf->err = NULL;
		g_hash_table_foreach(rf->hrname, fetch_feed, statuscb);	
		// reset cancelation signal
		if (rf->cancel)
			rf->cancel = 0;
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
	if (rf->mozembed)
		gtk_widget_destroy(rf->mozembed);
#ifdef HAVE_GTKMOZEMBED
//	gtk_moz_embed_pop_startup ();
#endif
//	gtk_moz_embed_destroy(rf->mozembed);
//	GtkMozEmbed *a = rf->mozembed;
//	a->data->Destroy();
//	a->priv->browser->Destroy();
	g_print(".done\n");
	guint render = GPOINTER_TO_INT(
	gconf_client_get_int(rss_gconf, 
			GCONF_KEY_HTML_RENDER, 
			NULL));
	//really find a better way to deal with this//
	if (2 == render)
		system("killall -SIGTERM evolution");
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
		printf("RSS Plugin enabled\n");
		//initiate main rss structure
		if (!rf)
		{
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
			get_feed_folders();
#if HAVE_DBUS
#if RSS_DEBUG
			g_print("init_dbus()\n");
#endif
			/*D-BUS init*/
			rf->bus = init_dbus ();
#endif
			atexit(rss_finalize);
			guint render = GPOINTER_TO_INT(
			gconf_client_get_int(rss_gconf, 
						GCONF_KEY_HTML_RENDER, 
						NULL));
		
			//render = 0 means gtkhtml however it could mean no value set
			//perhaps we should change this number representing gtkhtml

			if (!render) 	// set render just in case it was forced in configure
			{
				render = RENDER_N;
  				gconf_client_set_int(rss_gconf, 
						GCONF_KEY_HTML_RENDER, render, NULL);
			}
#ifdef HAVE_GTKMOZEMBED
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
	gchar *author = CF->q ? CF->q : CF->sender;

	mail_folder = check_feed_folder(CF->full_path);
	camel_object_ref(mail_folder);

        camel_folder_freeze(mail_folder);

	info = camel_message_info_new(NULL);
	camel_message_info_set_flags(info, CAMEL_MESSAGE_SEEN, 1);

	gchar *tmp = markup_decode(CF->subj);
	camel_mime_message_set_subject(new, tmp);
	g_free(tmp);

	addr = camel_internet_address_new(); 
#ifdef RSS_DEBUG
	g_print("date:%s\n", CF->date);
#endif
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
		else /*use now as time for failsafe*/
			camel_mime_message_set_date(new, CAMEL_MESSAGE_DATE_CURRENT, 0);
	}

	camel_medium_set_header(CAMEL_MEDIUM(new), "Website", CF->website);
	camel_medium_set_header(CAMEL_MEDIUM(new), "RSS-ID", CF->feedid);
	rtext = camel_data_wrapper_new ();
        type = camel_content_type_new ("text", "evolution-rss-feed");
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
		camel_multipart_add_part(mp, msgp);
		camel_object_unref(msgp);
	      	camel_medium_set_content_object((CamelMedium *)new, (CamelDataWrapper *)mp);
		camel_object_unref(mp);
	}
        else
		camel_medium_set_content_object(CAMEL_MEDIUM(new), CAMEL_DATA_WRAPPER(rtext));

	camel_folder_append_message(mail_folder, new, info, NULL, ex);
	camel_folder_sync(mail_folder, FALSE, NULL);
        camel_folder_thaw(mail_folder);
        camel_operation_end(NULL);
	camel_object_unref(rtext);
	camel_object_unref(new);
	camel_message_info_free(info);
	camel_object_unref(mail_folder);
}

/************ RDF Parser *******************/

static char *
layer_find_innerelement (xmlNodePtr node, 
	    char *match, char *el,
	    char *fail)
{
	while (node!=NULL) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (strcasecmp (node->name, match)==0) {
			return xmlGetProp(node, el);
		}
		node = node->next;
	}
	return fail;
}

xmlNode *
html_find (xmlNode *node,
            char *match)
{
#ifdef RDF_DEBUG
g_print("parser error 3_1!!!\n");
#endif
	while (node) {
#ifdef RDF_DEBUG
                xmlDebugDumpNode (stdout, node, 32);
                printf("%s.\n", node->name);
#endif
                if (node->children)
                        node = node->children;
                else {
                        while (node && !node->next)
                                node = node->parent;
                        //if (!node || node == top)
                        if (!node)
{
#ifdef RDF_DEBUG
g_print("parser error 3_2 -> return NULL!!!\n");
#endif
                                return NULL;
}
                        node = node->next;
                }

                if (node->name) {
                        if (!strcmp (node->name, match))
{
#ifdef RDF_DEBUG
g_print("parser error 3_3 -> return NULL!!!\n");
#endif
                                return node;
}
                }
        }
#ifdef RDF_DEBUG
g_print("parser error 3_4 -> return NULL!!!\n");
#endif
        return NULL;
}

static char *
layer_find (xmlNodePtr node, 
	    char *match, 
	    char *fail)
{
	while (node!=NULL) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (strcasecmp (node->name, match)==0) {
			if (node->children != NULL && node->children->content != NULL) {
				return node->children->content;
			} else {
				return fail;
			}
		}
		node = node->next;
	}
	return fail;
}

//
//namespace-based modularization
//standard modules
//
//	mod_content
//	* only handles content:encoding
//	* if it's necessary handle
//	  content:item stuff

gchar *
content_rss(xmlNode *node, gchar *fail)
{
	//guint len=0;
	//xmlBufferPtr buf = xmlBufferCreate();
	gchar *content;

	content = xmlNodeGetContent(node);
	if (content)
		return content;
	else
		return fail;
/*	len = xmlNodeDump(buf, node->doc, node->children->next, 0, 0);
	if (len)
	{
		content = g_strdup_printf("%s", xmlBufferContent(buf));
		xmlBufferFree(buf);
		return content;
	}
	else
		return fail;*/
}

void
dublin_core_rss(void)
{
	g_print("dublin core\n");
}

void
syndication_rss(void)
{
	g_print("syndication\n");
}


gchar *standard_rss_modules[3][3] = {
	{"content", "content", (gchar *)content_rss},
	{"dublin core", "dc", (gchar *)dublin_core_rss},
	{"syndication", "sy", (gchar *)syndication_rss}};


static char *
layer_find_tag (xmlNodePtr node,
            char *match,
            char *fail)
{
	xmlBufferPtr buf = xmlBufferCreate();
	gchar *content;
	guint len = 0;
	int i;
	char* (*func)();

        while (node!=NULL) {
#ifdef RDF_DEBUG
                xmlDebugDumpNode (stdout, node, 32);
                printf("%s.\n", node->name);
#endif
		if (node->ns && node->ns->prefix)
		{
//                	printf("ns:%s\n", node->ns->prefix);
			for (i=0; i < 3; i++)
			{
				if (!strcasecmp (node->ns->prefix, standard_rss_modules[i][1]))
				{
					func = (gpointer)standard_rss_modules[i][2];
					if (strcasecmp (node->ns->prefix, match)==0)
						return func(node, fail);
				}
			}
		}
                if (strcasecmp (node->name, match)==0) {
                        if (node->children != NULL && node->children->next != NULL) {
#ifdef RDF_DEBUG
				g_print("NODE DUMP:%s\n", xmlNodeGetContent(node->children->next));
#endif
				len = xmlNodeDump(buf, node->doc, node->children->next, 0, 0);
				content = g_strdup_printf("%s", xmlBufferContent(buf));
				xmlBufferFree(buf);
				return content;
                        } else {
				xmlBufferFree(buf);
                                return fail;
                        }
                }
                node = node->next;
        }
	xmlBufferFree(buf);
        return fail;
}

static gchar *
layer_find_innerhtml (xmlNodePtr node,
	    char *match, char *submatch,
	    char *fail)
{
	while (node!=NULL) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (strcasecmp (node->name, match)==0 && node->children) {
			return layer_find(node->children->next, submatch, fail);
		}
		node = node->next;
	}
	return fail;
}

xmlNodePtr
layer_find_pos (xmlNodePtr node,
            char *match, char *submatch)
{
        while (node!=NULL) {
#ifdef RDF_DEBUG
                xmlDebugDumpNode (stdout, node, 32);
                printf("%s.\n", node->name);
#endif
                if (strcasecmp (node->name, match)==0) {
                        return node;
                }
                node = node->children->next;
        }
        return NULL;
}

static char *
layer_find_url (xmlNodePtr node, 
		char *match, 
		char *fail)
{
	char *p = layer_find (node, match, fail);
	char *r = p;
	static char *wb = NULL;
	char *w;
	
	if (wb) {
		g_free (wb);
	}
	
	wb = w = g_malloc (3 * strlen (p));

	if (w == NULL) {
		return fail;
	}
	
	if (*r == ' ') r++;	/* Fix UF bug */

	while (*r) {
		if (strncmp (r, "&amp;", 5) == 0) {
			*w++ = '&';
			r += 5;
			continue;
		}
		if (strncmp (r, "&lt;", 4) == 0) {
			*w++ = '<';
			r += 4;
			continue;
		}
		if (strncmp (r, "&gt;", 4) == 0) {
			*w++ = '>';
			r += 4;
			continue;
		}
		if (*r == '"' || *r == ' '){
			*w++ = '%';
			*w++ = "0123456789ABCDEF"[*r/16];
			*w++ = "0123456789ABCDEF"[*r&15];
			r++;
			continue;
		}
		*w++ = *r++;
	}
	*w = 0;
	return wb;
}

gchar *
get_real_channel_name(gchar *uri, gchar *failed)
{
	gpointer crc_feed = gen_md5(uri);
	gchar *chn_name = g_hash_table_lookup(rf->hrname_r, crc_feed);
	g_free(crc_feed);
	return chn_name ? chn_name : failed;
}

gchar *
tree_walk (xmlNodePtr root, RDF *r)
{
	xmlNodePtr walk;
	xmlNodePtr rewalk = root;
	xmlNodePtr channel = NULL;
	xmlNodePtr image = NULL;
	GArray *item = g_array_new (TRUE, TRUE, sizeof (xmlNodePtr));
	char *t;
	char *charset;

	/* check in-memory encoding first, fallback to transport encoding, which may or may not be correct */
	if (r->cache->charset == XML_CHAR_ENCODING_UTF8
	    || r->cache->charset == XML_CHAR_ENCODING_ASCII) {
		charset = NULL;
	} else {
		/* bad/missing encoding, fallback to latin1 (locale?) */
		charset = r->cache->encoding ? (char *)r->cache->encoding : "iso-8859-1";
	}

	do {
		walk = rewalk;
		rewalk = NULL;
		
		while (walk!=NULL){
#ifdef RDF_DEBUG
			printf ("%p, %s\n", walk, walk->name);
#endif
			if (strcasecmp (walk->name, "rdf") == 0) {
//				xmlNode *node = walk;
				rewalk = walk->children;
				walk = walk->next;
				if (!r->type)
					r->type = g_strdup("RDF");
				r->type_id = RDF_FEED;
//                		gchar *ver = xmlGetProp(node, "version");
                		if (r->version)
					g_free(r->version);
				r->version = g_strdup("(RSS 1.0)");
//				if (ver)
//					xmlFree(ver);
				continue;
			}
			if (strcasecmp (walk->name, "rss") == 0){
				xmlNode *node = walk;
				rewalk = walk->children;
				walk = walk->next;
				if (!r->type)
				r->type = g_strdup("RSS");
				r->type_id = RSS_FEED;
                		gchar *ver = xmlGetProp(node, "version");
                		if (r->version)
					g_free(r->version);
				r->version = g_strdup(ver);
				if (ver)
					xmlFree(ver);
				continue;
			}
			if (strcasecmp (walk->name, "feed") == 0) {
				xmlNode *node = walk;
				if (!r->type)
				r->type = g_strdup("ATOM");
				r->type_id = ATOM_FEED;
                		gchar *ver = xmlGetProp(node, "version");
				if (ver)
				{
                			if (r->version)
						g_free(r->version);
					r->version = g_strdup(ver);
					xmlFree(ver);
				}
				else
				{
                			if (r->version)
						g_free(r->version);
					r->version = g_strdup("1.0");
				}
			}

			/* This is the channel top level */
#ifdef RDF_DEBUG
			printf ("Top level '%s'.\n", walk->name);
#endif
			if (strcasecmp (walk->name, "channel") == 0) {
				channel = walk;
				rewalk = channel->children;
			}
			if (strcasecmp (walk->name, "feed") == 0) {
				channel = walk;
				rewalk = channel->children;
			}
			if (strcasecmp (walk->name, "image") == 0) {
				image = walk;
			}
			if (strcasecmp (walk->name, "item") == 0) {
				g_array_append_val(item, walk);
			}
			if (strcasecmp (walk->name, "entry") == 0) {
				g_array_append_val(item, walk);
			}
			walk = walk->next;
		}
	}
	while (rewalk);
	
	if (channel == NULL) {
		fprintf(stderr, "No channel definition.\n");
		return NULL;
	}

	t = g_strdup(get_real_channel_name(r->uri, NULL));
	//feed might be added with no validation
	//so it could be named Untitled channel
	//till validation process
	if (t == NULL || !g_ascii_strncasecmp(t,
			DEFAULT_NO_CHANNEL, 
			strlen(DEFAULT_NO_CHANNEL))) 
	{
		t = layer_find(channel->children, 
				"title", 
				DEFAULT_NO_CHANNEL);
		t = decode_html_entities(t);	
		gchar *tmp = sanitize_folder(t);
		g_free(t);
		t = tmp;
		t = generate_safe_chn_name(t);
	}

	//items might not have a date
	// so try to grab channel/feed date
	gchar *md2 = g_strdup(layer_find(channel->children, "date", 
		layer_find(channel->children, "pubDate", 
		layer_find(channel->children, "updated", NULL))));

	r->feedid = update_channel(
			//atempt to find real_channel name using url
			t,
			r->uri,
			md2, 
			item);
	if (md2)
		g_free(md2);
	g_array_free(item, TRUE);
	g_free(r->feedid);
	return t;
}

CamelMimePart *
file_to_message(const char *name)
{
	g_return_if_fail (g_file_test(name, G_FILE_TEST_IS_REGULAR));
	const char *type;
        CamelStreamFs *file;
        CamelMimePart *msg = camel_mime_part_new();
	camel_mime_part_set_encoding(msg, CAMEL_TRANSFER_ENCODING_BINARY);
	CamelDataWrapper *content = camel_data_wrapper_new();
	
        file = (CamelStreamFs *)camel_stream_fs_new_with_name(name, O_RDONLY, 0);

        camel_data_wrapper_construct_from_stream(content, (CamelStream *)file);
        camel_object_unref((CamelObject *)file);
	camel_medium_set_content_object((CamelMedium *)msg, content);
        camel_object_unref(content);
	
	type = em_utils_snoop_type(msg);
	if (type)
		camel_data_wrapper_set_mime_type((CamelDataWrapper *)msg, type);

	camel_mime_part_set_filename(msg, name);
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
	g_free(CF->feed_fname);
	g_free(CF->feed_uri);
	g_free(CF);
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
	if (fr)
	{
	    while (fgets(rfeed, 511, fr) != NULL)
	    {
		if (rfeed && strstr(rfeed, needle))
		{
			occ=1;
			break;
		}
	    }
	    fclose(fr);
	}
	if (!occ)
	{
		FILE *fw = fopen(file_name, "a+");
		if (fw)
		{
			fputs(needle, fw);
			fclose(fw);
		}
	}	
	return occ;
}

static void
#if LIBSOUP_VERSION < 2003000
finish_enclosure (SoupMessage *msg, create_feed *user_data)
#else
finish_enclosure (SoupSession *soup_sess, SoupMessage *msg, create_feed *user_data)
#endif
{
	gchar *tmpdir = NULL;
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
		create_mail(user_data);
	free_cf(user_data);
}

//migrates old feed data files from crc naming
//to md5 naming while preserving content
//
//this will be obsoleted over a release or two

void
migrate_crc_md5(const char *name, gchar *url)
{
	u_int32_t crc = gen_crc(name);
	u_int32_t crc2 = gen_crc(url);
	gchar *md5 = gen_md5(url);

	gchar *feed_dir = g_strdup_printf("%s/mail/rss", 
	    mail_component_peek_base_directory (mail_component_peek ()));
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
update_channel(const char *chn_name, gchar *url, char *main_date, GArray *item)
{
        guint i;
	gchar *sender = g_strdup_printf("%s <%s>", chn_name, chn_name);
	CamelStore *store = mail_component_peek_local_store(NULL);
	char *d2 = NULL;
	xmlNodePtr el;
	char *q = NULL;
	char *b = NULL;
	gchar *feed = NULL;
	gboolean freeb = 0; //if b needs to be freed or not
	gchar *encl, *encl_file;

	migrate_crc_md5(chn_name, url);

	gchar *buf = gen_md5(url);

	gchar *feed_dir = g_strdup_printf("%s/mail/rss", 
	    mail_component_peek_base_directory (mail_component_peek ()));
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
	    g_mkdir_with_parents (feed_dir, 0755);

	gchar *feed_name = g_strdup_printf("%s/%s", feed_dir, buf);
	g_free(feed_dir);
	
	FILE *fr = fopen(feed_name, "r");
	FILE *fw = fopen(feed_name, "a+");

	for (i=0; NULL != (el = g_array_index(item, xmlNodePtr, i)); i++)
	{
                char *p = layer_find (el->children, "title", "Untitled article");
		//firstly try to parse as an ATOM author
               	char *q1 = g_strdup(layer_find_innerhtml (el->children, "author", "name", NULL));
		char *q2 = g_strdup(layer_find_innerhtml (el->children, "author", "uri", NULL));
		char *q3 = g_strdup(layer_find_innerhtml (el->children, "author", "email", NULL));
		if (q1)
		{
        		q1 = g_strdelimit(q1, "><", ' ');
			if (q3)
			{
        			q3 = g_strdelimit(q3, "><", ' ');
               			q = g_strdup_printf("%s <%s>", q1, q3);
				g_free(q1);
				if (q2) g_free(q2);
				g_free(q3);
			}
			else
			{
				if (q2)
        				q2 = g_strdelimit(q2, "><", ' ');
				else 
					q2 = g_strdup(q1);
               			q = g_strdup_printf("%s <%s>", q1, q2);
				g_free(q1);
				g_free(q2);
			}
		}
		else	//then RSS or RDF
		{
                	q = g_strdup(layer_find (el->children, "author", 
				layer_find (el->children, "creator", NULL)));
			if (q)
			{
				//evo will go crazy when it'll encounter ":" character
        			//it probably enforces strict rfc2047 compliance
        			q = g_strdelimit(q, "><:", ' ');
        			gchar *tmp = g_strdup_printf("\"%s\" <\"%s\">", q, q);
				g_free(q);
				q = tmp;
				if (q2) g_free(q2);
				if (q3) g_free(q3);
			}
		}
		//FIXME this might need xmlFree when namespacing
		b = layer_find_tag (el->children, "description",
				layer_find_tag (el->children, "content", NULL));

		if (!b)
                	b = g_strdup(layer_find (el->children, "description",
				layer_find (el->children, "content",
				layer_find (el->children, "summary", "No information"))));

                char *d = layer_find (el->children, "pubDate", NULL);
		//date in dc module format
		if (!d)
		{
                	d2 = layer_find (el->children, "date", NULL);					//RSS2
			if (!d2)
			{
				d2 = layer_find(el->children, "updated", NULL); 			//ATOM
				if (!d2) //take channel date if exists
					d2 = main_date;
			}
		}

		encl = layer_find_innerelement(el->children, "enclosure", "url",			// RSS 2.0 Enclosure
			layer_find_innerelement(el->children, "link", "enclosure", NULL)); 		// ATOM Enclosure
		//we have to free this some how
                char *link = g_strdup(layer_find (el->children, "link", NULL));			//RSS,
		if (!link) 
			link = layer_find_innerelement(el->children, "link", "href", g_strdup(_("No Information")));	//ATOM
		char *id = layer_find (el->children, "id",				//ATOM
				layer_find (el->children, "guid", NULL));		//RSS 2.0
		feed = g_strdup_printf("%s\n", id ? id : link);
#ifdef RSS_DEBUG
		g_print("link:%s\n", link);
		g_print("body:%s\n", b);
		g_print("author:%s\n", q);
		g_print("sender:%s\n", sender);
		g_print("title:%s\n", p);
		g_print("date:%s\n", d);
		g_print("date:%s\n", d2);
#endif
		p =  decode_html_entities (p);
		gchar *tmp = decode_html_entities(b);
		g_free(b);
		b = tmp;
			
		gchar rfeed[513];
		memset(rfeed, 0, 512);
		int occ = 0;
		while (gtk_events_pending())
                  gtk_main_iteration ();

		if (fr)
		{
		    while (fgets(rfeed, 511, fr) != NULL)
		    {
			if (rfeed && strstr(rfeed, feed))
			{
				occ=1;
				break;
			}
		    }
		    (void)fseek(fr, 0L, SEEK_SET);
		}

		while (gtk_events_pending())
                  gtk_main_iteration ();

		if (!occ)
		{
			create_feed *CF = g_new0(create_feed, 1);	
			/* pack all data */
			CF->full_path 	= g_strdup(chn_name);
			CF->q 		= g_strdup(q);
			CF->sender 	= g_strdup(sender);
			CF->subj 	= g_strdup(p);
			CF->body 	= g_strdup(b);
			CF->date 	= g_strdup(d);
			CF->dcdate 	= g_strdup(d2);
			CF->website 	= g_strdup(link);
			CF->feedid 	= g_strdup(buf);
			CF->encl 	= g_strdup(encl);
			CF->feed_fname  = g_strdup(feed_name);		//feed file name
			CF->feed_uri	= g_strdup(feed);		//feed file url (to be checked/written to feed file)
				
			if (encl)
			{
				GError *err = NULL;
				net_get_unblocking(
                        	        encl,
                        	        textcb,
                                	NULL,
                                	(gpointer)finish_enclosure,
                                	CF,
                                	&err);
			}
			else
			{
				if (fw) fputs(feed, fw);
   	    	    			create_mail(CF);
				free_cf(CF);
			}
		}

#ifdef RSS_DEBUG
		g_print("put success()\n");
#endif
tout:		if (q) g_free(q);
		g_free(b);
		g_free(p);
		if (feed) g_free(feed);
		if (encl) g_free(encl);
		g_free(link);
        }
out:	g_free(sender);

	if (fr) fclose(fr);
	if (fw) fclose(fw);
	
	g_free(feed_name);
	return buf;
}

gchar *
display_doc (RDF *r)
{
	xmlNodePtr root = xmlDocGetRootElement (r->cache);
	return tree_walk (root, r);
}

static void
delete_oldest_article(CamelFolder *folder, guint unread)
{
	CamelMessageInfo *info;
	GPtrArray *uids;
	guint i, j = 0, imax;
	guint32 flags;
	time_t date, min_date = 0;
	uids = camel_folder_get_uids (folder);
       	for (i = 0; i < uids->len; i++)
	{
		info = camel_folder_get_message_info(folder, uids->pdata[i]);
               	if (info && rf->current_uid != uids->pdata[i]) {
			date = camel_message_info_date_sent(info);
			flags = camel_message_info_flags(info);
       			if (flags & CAMEL_MESSAGE_SEEN)
			{
				
				if (!j++)
				{
					min_date = date;
					imax = i;
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
					if (!j++)
					{
                                       		min_date = date;
						imax = i;
					}
                               		if (date < min_date)
                               		{
                                       		imax = i;
                                       		min_date = date;
                               		}
				}
			}
              	camel_message_info_free(info);
               	}
	}
       	camel_folder_freeze(folder);
	if (min_date)
		camel_folder_delete_message (folder, uids->pdata[imax]);
      	camel_folder_sync (folder, TRUE, NULL);
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
#ifdef RSS_DEBUG
	g_print("Cleaning folder: %s\n", real_folder);
#endif

        gchar *real_name = g_strdup_printf("%s/%s", lookup_main_folder(), real_folder);
	if (!(folder = camel_store_get_folder (store, real_name, 0, NULL)))
                        goto fail;
	time (&now);
	
	guint del_unread = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_unread, value));
	guint del_feed = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_feed, value));
	if (del_feed == 2)
	{	
		guint del_days = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_days, value));
		uids = camel_folder_get_uids (folder);
        	camel_folder_freeze(folder);
        	for (i = 0; i < uids->len; i++)
		{
			info = camel_folder_get_message_info(folder, uids->pdata[i]);
                	if (info && rf->current_uid != uids->pdata[i]) {
				date = camel_message_info_date_sent(info);
				if (date < now - del_days * 86400)
				{
					flags = camel_message_info_flags(info);
                               		if (!(flags & CAMEL_MESSAGE_SEEN))
					{
						if (del_unread)
							camel_message_info_set_flags(info, CAMEL_MESSAGE_SEEN|CAMEL_MESSAGE_DELETED, ~0);
					}
					else
						camel_message_info_set_flags(info, CAMEL_MESSAGE_DELETED, ~0);
				}
                        	camel_folder_free_message_info(folder, info);
                	}
		}
        	camel_folder_sync (folder, TRUE, NULL);
        	camel_folder_thaw(folder);
        	camel_folder_free_uids (folder, uids);
	}
	if (del_feed == 1)
	{
		guint del_messages = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_messages, value));
		guint total = camel_folder_get_message_count(folder);
		i=1;
		while (del_messages < camel_folder_get_message_count(folder) && i <= total)
		{
			delete_oldest_article(folder, del_unread);
			i++;
		}
	}


	total = camel_folder_get_message_count (folder);
	camel_object_unref (folder);
	g_print("=> total:%d\n", total);
fail:	g_free(real_name);
}

