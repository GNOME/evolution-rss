/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007  Lucian Langa <cooly@mips.edu.ms> 
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
//#include <libxml/parser.h>
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
#include <libsoup/soup-message-queue.h>

#include "rss.h"
#include "network-soup.c"
#include "misc.c"
#if HAVE_DBUS
#include "dbus.c"
#endif

int pop = 0;
//#define RSS_DEBUG 1
#define d(x)


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
guint count = 0;
gchar *buffer = NULL;

#define RSS_CONTROL_ID  "OAFIID:GNOME_Evolution_RSS:" EVOLUTION_VERSION_STRING
#define FACTORY_ID      "OAFIID:GNOME_Evolution_RSS_Factory:" EVOLUTION_VERSION_STRING

guint           upgrade = 0;                // set to 2 when initailization successfull

gboolean setup_feed(add_feed *feed);
gchar *display_doc (RDF *r);
void check_folders(void);
gboolean update_articles(gboolean disabler);
//u_int32_t 
gchar *
update_channel(const char *chn_name, char *url, char *main_date, GArray *item);
static char *layer_find (xmlNodePtr node, char *match, char *fail);
static char *layer_find_innerelement (xmlNodePtr node, char *match, char *el, char *fail);
static gchar *layer_find_innerhtml (xmlNodePtr node, char *match, char *submatch, char *fail);
xmlNodePtr layer_find_pos (xmlNodePtr node, char *match, char *submatch);
static xmlNode *html_find (xmlNode *node, char *match);
gchar *strplchr(gchar *source);
static char *gen_md5(gchar *buffer);
static void feeds_dialog_edit(GtkDialog *d, gpointer data);
CamelMimePart *file_to_message(const char *name);
gchar *lookup_main_folder(void);
void populate_reversed(gpointer key, gpointer value, GHashTable *hash);
gchar *get_real_channel_name(gchar *uri, gchar *failed);
gchar *lookup_feed_folder(gchar *folder);
void save_gconf_feed(void);
void check_feed_age(void);
static gboolean check_if_match (gpointer key, gpointer value, gpointer user_data);
gchar *decode_html_entities(gchar *str);
void delete_feed_folder_alloc(gchar *old_name);
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

/* I could really use this stuff exported through evolution include */

struct _ActivityInfo {
        char *component_id;
        GdkPixbuf *icon_pixbuf;
        guint id;
        char *information;
        gboolean cancellable;
        double progress;
        GtkWidget *menu;
        void (*cancel_func) (gpointer data);
        gpointer data;
        gpointer error;
        time_t  error_time;
};
typedef struct _ActivityInfo ActivityInfo;

struct _EActivityHandlerPrivate {
        guint next_activity_id;
        GList *activity_infos;
        GSList *task_bars;
//        ELogger *logger;
        guint error_timer;
        guint error_flush_interval;

};

static GList *
lookup_activity (GList *list,
                 guint activity_id,
                 int *order_number_return)
{
        GList *p;
        int i;

        for (p = list, i = 0; p != NULL; p = p->next, i ++) {
                ActivityInfo *activity_info;

                activity_info = (ActivityInfo *) p->data;
                if (activity_info->id == activity_id) {
                        *order_number_return = i;
                        return p;
                }
        }

        *order_number_return = -1;
        return NULL;
}

void
taskbar_op_set_progress(gpointer key, gdouble progress)
{
	EActivityHandler *activity_handler = mail_component_peek_activity_handler (mail_component_peek ());
	guint activity_id = g_hash_table_lookup(rf->activity, key);

	if (activity_id)
	{
	
		/* does it even makes sense to setup information everytime progress is updated ??? */
		EActivityHandlerPrivate *priv = activity_handler->priv;
        	ActivityInfo *activity_info;
        	GList *p;
		int order_number;
		g_hash_table_foreach(rf->activity, print_hash, NULL);
	
        	p = lookup_activity (priv->activity_infos, activity_id, &order_number);
        	if (p == NULL) {
                	g_warning ("EActivityHandler: unknown operation %d", activity_id);
                	return;
        	}

        	activity_info = (ActivityInfo *) p->data;
	g_print("--message:%s--\n", activity_info->information);

		e_activity_handler_operation_progressing(activity_handler,
				activity_id,
                                g_strdup(activity_info->information), 
                                progress);
	}
}

void
taskbar_op_finish(gpointer key)
{
	EActivityHandler *activity_handler = mail_component_peek_activity_handler (mail_component_peek ());
	
	if (rf->activity)
	{
		guint activity_key = g_hash_table_lookup(rf->activity, key);
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
//		taskbar_op_set_progress(data, (guint)fraction);
//		taskbar_op_set_progress(data, fraction/100);
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
/*    NetStatusProgress *progress;
    float fraction = 0;
    switch (status) {
    case NET_STATUS_PROGRESS:
        progress = (NetStatusProgress*)statusdata;
        if (progress->current > 0 && progress->total > 0) {
	fraction = (float)progress->current / progress->total;
#ifdef RSS_DEBUG
	g_print("%f.", fraction*100);
#endif
	g_print("%f->", fraction*100);
	g_print("%f->", progress->current);
	g_print("%f.\n", progress->total);
	}
	while (gtk_events_pending())
      		gtk_main_iteration ();
        break;
    default:
        g_warning("unhandled network status %d\n", status);
    }*/
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

add_feed *
create_dialog_add(gchar *text, gchar *feed_text)
{
  GtkWidget *dialog1;
  GtkWidget *dialog_vbox1;
  GtkWidget *vbox1;
  GtkWidget *hbox1;
  GtkWidget *label1;
  GtkWidget *label2;
  GtkWidget *entry1;
  GtkWidget *checkbutton1;
  GtkWidget *checkbutton2;
  GtkWidget *checkbutton3, *checkbutton4;
  GtkWidget *dialog_action_area1;
  GtkWidget *cancelbutton1;
  GtkWidget *okbutton1;
  add_feed *feed = g_new0(add_feed, 1);
  gboolean fhtml = FALSE;
  gboolean enabled = TRUE;
  gboolean del_unread = FALSE;
  guint del_feed = 0;
  guint del_days = 10;
  guint del_messages = 10;
  GtkAccelGroup *accel_group = gtk_accel_group_new ();
  gchar *flabel = NULL;

  dialog1 = gtk_dialog_new ();
  gtk_window_set_keep_above(GTK_WINDOW(dialog1), TRUE);

  if (text != NULL)
  	gtk_window_set_title (GTK_WINDOW (dialog1), _("Edit Feed"));
  else
  	gtk_window_set_title (GTK_WINDOW (dialog1), _("Add Feed"));
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog1), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (dialog1), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_modal (GTK_WINDOW (dialog1), FALSE);

  dialog_vbox1 = GTK_DIALOG (dialog1)->vbox;
  gtk_widget_show (dialog_vbox1);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, TRUE, TRUE, 0);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox1), 9);

  label2 = gtk_label_new (_("Feed URL: "));
  gtk_widget_show (label2);
  gtk_box_pack_start (GTK_BOX (hbox1), label2, FALSE, FALSE, 0);

  entry1 = gtk_entry_new ();
  gtk_widget_show (entry1);
  gtk_box_pack_start (GTK_BOX (hbox1), entry1, TRUE, TRUE, 0);
  gtk_entry_set_invisible_char (GTK_ENTRY (entry1), 8226);
  //editing
  if (text != NULL)
  {
	gtk_entry_set_text(GTK_ENTRY(entry1), text);
	fhtml = GPOINTER_TO_INT(
		g_hash_table_lookup(rf->hrh, 
				lookup_key(feed_text)));
	enabled = GPOINTER_TO_INT(
		g_hash_table_lookup(rf->hre, 
				lookup_key(feed_text)));
	del_feed = GPOINTER_TO_INT(
		g_hash_table_lookup(rf->hrdel_feed, 
				lookup_key(feed_text)));
	del_unread = GPOINTER_TO_INT(
		g_hash_table_lookup(rf->hrdel_unread, 
				lookup_key(feed_text)));
	feed->del_days = GPOINTER_TO_INT(
		g_hash_table_lookup(rf->hrdel_days, 
				lookup_key(feed_text)));
	feed->del_messages = GPOINTER_TO_INT(
		g_hash_table_lookup(rf->hrdel_messages, 
				lookup_key(feed_text)));
  }

  gboolean validate = 1;


  GtkWidget *entry2;
  if (text != NULL)
  {
	GtkWidget *hboxt = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hboxt);
	gtk_box_pack_start (GTK_BOX (vbox1), hboxt, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hboxt), 9);

	flabel = g_strdup_printf("%s: <b>%s</b>", _("Folder"),
			lookup_feed_folder(feed_text));
	GtkWidget *labelt = gtk_label_new (flabel);
	gtk_label_set_use_markup(GTK_LABEL(labelt), 1);
	gtk_widget_show (labelt);
	gtk_box_pack_start (GTK_BOX (hboxt), labelt, FALSE, FALSE, 0);
  }
  else
  {
  	entry2 = gtk_label_new (NULL);
	gtk_widget_show (entry2);
	gtk_box_pack_start (GTK_BOX (vbox1), entry2, TRUE, TRUE, 0);
	gtk_entry_set_invisible_char (GTK_ENTRY (entry2), 8226);
  }

  label1 = gtk_label_new (_("<b>Articles Settings</b>"));
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (vbox1), label1, FALSE, FALSE, 0);
  gtk_label_set_use_markup (GTK_LABEL (label1), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label1), 0.0, 0.5);

  checkbutton1 = gtk_check_button_new_with_mnemonic (
		_("Show article's summary"));
  gtk_widget_show (checkbutton1);
  gtk_box_pack_start (GTK_BOX (vbox1), checkbutton1, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton1), 1-fhtml);

  checkbutton2 = gtk_check_button_new_with_mnemonic (
		_("Feed Enabled"));
  gtk_widget_show (checkbutton2);
  gtk_box_pack_start (GTK_BOX (vbox1), checkbutton2, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton2), enabled);

  checkbutton3 = gtk_check_button_new_with_mnemonic (
		_("Validate feed"));
  if (text)
  	gtk_widget_set_sensitive(checkbutton3, FALSE);

  gtk_widget_show (checkbutton3);
  gtk_box_pack_start (GTK_BOX (vbox1), checkbutton3, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton3), validate);


GtkWidget *hbox2, *label3;
GtkWidget *radiobutton1, *radiobutton2, *radiobutton3;
GtkWidget *spinbutton1, *spinbutton2;
GtkObject *spinbutton1_adj, *spinbutton2_adj;
GSList *radiobutton1_group = NULL;

 //editing
// if (text != NULL)
// {
  label1 = gtk_label_new (_("<b>Articles Storage</b>"));
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (vbox1), label1, FALSE, FALSE, 0);
  gtk_label_set_use_markup (GTK_LABEL (label1), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label1), 0.0, 0.5);
  radiobutton1 = gtk_radio_button_new_with_mnemonic (NULL, _("Don't delete articles"));
  gtk_widget_show (radiobutton1);
  gtk_box_pack_start (GTK_BOX (vbox1), radiobutton1, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton1), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));
  hbox1 = gtk_hbox_new (FALSE, 10);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
  radiobutton2 = gtk_radio_button_new_with_mnemonic (NULL, _("Delete all but the last"));
  gtk_widget_show (radiobutton2);
  gtk_box_pack_start (GTK_BOX (hbox1), radiobutton2, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton2), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));
  spinbutton1_adj = gtk_adjustment_new (10, 1, 1000, 1, 10, 10);
  spinbutton1 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton1_adj), 1, 0);
  gtk_widget_show (spinbutton1);
  if (feed->del_messages)
  	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton1), feed->del_messages);
  g_signal_connect(spinbutton1, "changed", G_CALLBACK(del_messages_cb), feed);
  gtk_box_pack_start (GTK_BOX (hbox1), spinbutton1, FALSE, TRUE, 0);
  label2 = gtk_label_new (_("messages"));
  gtk_widget_show (label2);
  gtk_box_pack_start (GTK_BOX (hbox1), label2, FALSE, FALSE, 0);
  hbox2 = gtk_hbox_new (FALSE, 10);
  gtk_widget_show (hbox2);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox2, FALSE, FALSE, 0);
  radiobutton3 = gtk_radio_button_new_with_mnemonic (NULL, _("Delete articles older than"));
  gtk_widget_show (radiobutton3);
  gtk_box_pack_start (GTK_BOX (hbox2), radiobutton3, FALSE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton3), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));
  switch (del_feed)
  {
	case 1:		//all but the last
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(radiobutton2), 1);
		break;
	case 2:		//older than days
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(radiobutton3), 1);
		break;
	default:
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(radiobutton1), 1);
  }
  spinbutton2_adj = gtk_adjustment_new (10, 1, 365, 1, 10, 10);
  spinbutton2 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton2_adj), 1, 0);
  if (feed->del_days)
  	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton2), feed->del_days);
  gtk_widget_show (spinbutton2);
  g_signal_connect(spinbutton2, "changed", G_CALLBACK(del_days_cb), feed);
  gtk_box_pack_start (GTK_BOX (hbox2), spinbutton2, FALSE, FALSE, 0);
  label3 = gtk_label_new (_("day(s)"));
  gtk_widget_show (label3);
  gtk_box_pack_start (GTK_BOX (hbox2), label3, FALSE, FALSE, 0);
  checkbutton4 = gtk_check_button_new_with_mnemonic (_("Always delete unread articles"));
  gtk_widget_show (checkbutton4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton4), del_unread);
  gtk_box_pack_start (GTK_BOX (vbox1), checkbutton4, FALSE, FALSE, 0);
// }

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
  gtk_widget_add_accelerator (okbutton1, "activate", accel_group,
                              GDK_KP_Enter, (GdkModifierType) 0,
                              GTK_ACCEL_VISIBLE);
  gtk_window_add_accel_group (GTK_WINDOW (dialog1), accel_group);

  gint result = gtk_dialog_run(GTK_DIALOG(dialog1));
  switch (result)
  {
    case GTK_RESPONSE_OK:
	feed->feed_url = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry1)));
	fhtml = gtk_toggle_button_get_active (
		GTK_TOGGLE_BUTTON (checkbutton1));
	fhtml ^= 1;
	feed->fetch_html = fhtml;
	enabled = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(checkbutton2));
	feed->enabled = enabled;
	validate = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(checkbutton3));
	feed->validate = validate;
//if (text)
//{
	guint i=0;
	while (i<3) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton1)))
			break;
		i++;
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton2)))
			break;
		i++;
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton3)))
			break;
	}
	feed->del_feed=i;
	feed->del_unread = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(checkbutton4));
	feed->del_messages = gtk_spin_button_get_value((GtkSpinButton *)spinbutton1);
	feed->del_days = gtk_spin_button_get_value((GtkSpinButton *)spinbutton2);
//}
	feed->add = 1;
	// there's no reason to feetch feed if url isn't changed
	if (text && !strncmp(text, feed->feed_url, strlen(text)))
		feed->changed = 0;
	else
		feed->changed = 1;
       	break;
    default:
	feed->add = 0;
	gtk_widget_destroy (dialog1);
	break;
  }
	feed->dialog = dialog1;
  if (flabel)
	g_free(flabel);
  return feed;
}

static void
construct_list(gpointer key, gpointer value, gpointer user_data)
{
	GtkListStore  *store = user_data;
	GtkTreeIter    iter;

	gtk_list_store_append (store, &iter);
  	gtk_list_store_set (store, &iter,
		0, g_hash_table_lookup(rf->hre, lookup_key(key)),
       		1, key, 
     		2, g_hash_table_lookup(rf->hrt, lookup_key(key)),
      		-1);
}

gboolean
cancel_soup_sess(gpointer key, gpointer value, gpointer user_data)
{
	g_print("key:%p, value:%p ==", key, value);

	if (SOUP_IS_SESSION(key))
	{
		if (SOUP_IS_MESSAGE(value))
		{
			soup_message_set_status(value,  SOUP_STATUS_CANCELLED);
			soup_session_cancel_message(key, value);
		}
		soup_session_abort(key);
		g_hash_table_find(rf->key_session,
                	remove_if_match,
                	user_data);
	}
	g_print(" key:%p, value:%p\n", key, value);
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
	if (rf->abort_session)
	{
		g_hash_table_foreach(rf->abort_session, remove_weak, NULL);
		g_hash_table_foreach_remove(rf->abort_session, cancel_soup_sess, NULL);
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
			soup_message_set_status(rf->b_msg_session, SOUP_STATUS_CANCELLED);
			soup_session_cancel_message(rf->b_session, rf->b_msg_session);
		}
		soup_session_abort(rf->b_session);
		rf->b_session = NULL;
		rf->b_msg_session = NULL;
	}
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

	abort_all_soup();
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

static void
feeds_dialog_add(GtkDialog *d, gpointer data)
{
	gchar *text;
	add_feed *feed = create_dialog_add(NULL, NULL);
	if (feed->feed_url && strlen(feed->feed_url))
        {
                text = feed->feed_url;
                feed->feed_url = sanitize_url(feed->feed_url);
                g_free(text);
                if (g_hash_table_find(rf->hr,
                                        check_if_match,
                                        feed->feed_url))
                {
                           rss_error(NULL, NULL, _("Error adding feed."),
                                           _("Feed already exists!"));
                           goto out;
                }
                setup_feed(feed);
        	GtkTreeModel *model = gtk_tree_view_get_model ((GtkTreeView *)data);
        	gtk_list_store_clear(GTK_LIST_STORE(model));
        	g_hash_table_foreach(rf->hrname, construct_list, model);
        	save_gconf_feed();
	}
out:	if (feed->dialog)
                gtk_widget_destroy(feed->dialog);
	g_free(feed);
}

static void
treeview_row_activated(GtkTreeView *treeview,
                       GtkTreePath *path, GtkTreeViewColumn *column)
{
	feeds_dialog_edit((GtkDialog *)treeview, treeview);
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

static void
feeds_dialog_edit(GtkDialog *d, gpointer data)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gchar *name, *feed_name;
	gchar *text;
	gchar *url;

	/* This will only work in single or browse selection mode! */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data));
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get (model, &iter, 1, &feed_name, -1);
		name = g_hash_table_lookup(rf->hr, lookup_key(feed_name));
		if (name)
		{
			add_feed *feed = create_dialog_add(name, feed_name);
			if (!feed->add)
				goto out;
			text = feed->feed_url;
			feed->feed_url = sanitize_url(feed->feed_url);
			g_free(text);
			url = name;
			if (feed->feed_url)
			{
				gtk_tree_model_get (model, &iter, 1, &name, -1);
				gpointer key = lookup_key(name);
				if (strcmp(url, feed->feed_url))
				{
					//prevent adding of an existing feed (url)
					//which might screw things
					if (g_hash_table_find(rf->hr,
						check_if_match,
						feed->feed_url))
					{
						rss_error(NULL, NULL, _("Error adding feed."), 
							_("Feed already exists!"));
						goto out;
					}
					gchar *value1 = g_strdup(g_hash_table_lookup(rf->hr, key));
//					remove_feed_hash(name);
					g_hash_table_remove(rf->hr, key);
					gpointer md5 = gen_md5(feed->feed_url);
					if (!setup_feed(feed))
					{
						//editing might loose a corectly setup feed
						//so re-add previous deleted feed
						g_hash_table_insert(rf->hr, g_strdup(key), value1);
					}
					else
						g_free(value1);
					gtk_list_store_clear(GTK_LIST_STORE(model));
					g_hash_table_foreach(rf->hrname, construct_list, model);
					save_gconf_feed();
					g_free(md5);
				}
				else
				{
					key = gen_md5(url);
					g_hash_table_replace(rf->hrh, 
							g_strdup(key), 
							GINT_TO_POINTER(feed->fetch_html));
					g_hash_table_replace(rf->hre, 
							g_strdup(key), 
							GINT_TO_POINTER(feed->enabled));
					g_hash_table_replace(rf->hrdel_feed, 
							g_strdup(key), 
							GINT_TO_POINTER(feed->del_feed));
					g_hash_table_replace(rf->hrdel_days, 
							g_strdup(key), 
							GINT_TO_POINTER(feed->del_days));
					g_hash_table_replace(rf->hrdel_messages, 
							g_strdup(key), 
							GINT_TO_POINTER(feed->del_messages));
					g_hash_table_replace(rf->hrdel_unread, 
							g_strdup(key), 
							GINT_TO_POINTER(feed->del_unread));
					g_free(key);
					gtk_list_store_clear(GTK_LIST_STORE(model));
					g_hash_table_foreach(rf->hrname, construct_list, model);
					save_gconf_feed();
				}
			}
out:			if (feed->dialog)
				gtk_widget_destroy(feed->dialog);
			g_free(feed);
		}
	}
}

void
remove_feed_hash(gpointer name)
{
	g_hash_table_remove(rf->hre, lookup_key(name));
	g_hash_table_remove(rf->hrt, lookup_key(name));
	g_hash_table_remove(rf->hrh, lookup_key(name));
	g_hash_table_remove(rf->hr, lookup_key(name));
	g_hash_table_remove(rf->hrname_r, lookup_key(name));
	g_hash_table_remove(rf->hrname, name);
}

void
rss_select_folder(gchar *folder_name)
{
	CamelStore *store = mail_component_peek_local_store(NULL);
	EMFolderTreeModel *model = mail_component_peek_tree_model(mail_component_peek());
        gchar *real_name = g_strdup_printf("%s/%s", lookup_main_folder(), folder_name);
        CamelFolder *folder = camel_store_get_folder (store, real_name, 0, NULL);

	g_print("real_name:%s\n", real_name);
        gchar *uri = mail_tools_folder_to_url (folder);
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

GtkWidget*
remove_feed_dialog(gchar *msg)
{
  GtkWidget *dialog1;
  GtkWidget *dialog_vbox1;
  GtkWidget *vbox1;
  GtkWidget *label1;
  GtkWidget *checkbutton1;
  GtkWidget *dialog_action_area1;
  GtkWidget *cancelbutton1;
  GtkWidget *okbutton1;

  dialog1 = gtk_dialog_new ();
  gtk_window_set_keep_above(GTK_WINDOW(dialog1), TRUE);
  gtk_window_set_title (GTK_WINDOW (dialog1), _("Delete Feed?"));
  gtk_window_set_type_hint (GTK_WINDOW (dialog1), GDK_WINDOW_TYPE_HINT_DIALOG);

  dialog_vbox1 = GTK_DIALOG (dialog1)->vbox;
  gtk_widget_show (dialog_vbox1);

  vbox1 = gtk_vbox_new (FALSE, 10);
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 10);

  label1 = gtk_label_new (msg);
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (vbox1), label1, TRUE, TRUE, 0);
  gtk_label_set_use_markup (GTK_LABEL (label1), TRUE);
  gtk_label_set_justify (GTK_LABEL (label1), GTK_JUSTIFY_CENTER);

  checkbutton1 = gtk_check_button_new_with_mnemonic (_("Remove folder contents"));
  gtk_widget_show (checkbutton1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton1),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_REMOVE_FOLDER, NULL));
  g_signal_connect(checkbutton1,
		"clicked",
		G_CALLBACK(start_check_cb),
		GCONF_KEY_REMOVE_FOLDER);
  gtk_box_pack_start (GTK_BOX (vbox1), checkbutton1, FALSE, FALSE, 0);

  dialog_action_area1 = GTK_DIALOG (dialog1)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);

  okbutton1 = gtk_button_new_from_stock ("gtk-delete");
  gtk_widget_show (okbutton1);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog1), okbutton1, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton1, GTK_CAN_DEFAULT);

  cancelbutton1 = gtk_button_new_with_label (_("Do not delete"));
  gtk_widget_show (cancelbutton1);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog1), cancelbutton1, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton1, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS (cancelbutton1, GTK_HAS_FOCUS);
  return dialog1;
}

//this function resembles emfu_delete_rec in mail/em-folder-utils.c
//which is not exported ? 
//
static void
rss_delete_rec (CamelStore *store, CamelFolderInfo *fi, CamelException *ex)
{
        while (fi) {
                CamelFolder *folder;

//                if (fi->child) {
  //                      rss_delete_rec (store, fi->child, ex);
    //                    if (camel_exception_is_set (ex))
      //                          return;
        //        }

                d(printf ("deleting folder '%s'\n", fi->full_name));
                printf ("deleting folder '%s'\n", fi->full_name);

                /* shouldn't camel do this itself? */
//                if (camel_store_supports_subscriptions (store))
  //                      camel_store_unsubscribe_folder (store, fi->full_name, NULL);

                if (!(folder = camel_store_get_folder (store, fi->full_name, 0, ex)))
                        return;

//                if (!CAMEL_IS_VEE_FOLDER (folder)) {
                        GPtrArray *uids = camel_folder_get_uids (folder);
                        int i;

                        camel_folder_freeze (folder);
                        for (i = 0; i < uids->len; i++)
                                camel_folder_delete_message (folder, uids->pdata[i]);

                        camel_folder_free_uids (folder, uids);

                        camel_folder_sync (folder, TRUE, NULL);
                        camel_folder_thaw (folder);
  //              }

                camel_store_delete_folder (store, fi->full_name, ex);
                if (camel_exception_is_set (ex))
                        return;

                fi = fi->next;
        }
}

static void
rss_delete_folders (CamelStore *store, const char *full_name, CamelException *ex)
{
        guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE | CAMEL_STORE_FOLDER_INFO_FAST | CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
        CamelFolderInfo *fi;

        fi = camel_store_get_folder_info (store, full_name, flags, ex);
        if (camel_exception_is_set (ex))
                return;

        rss_delete_rec (store, fi, ex);
        camel_store_free_folder_info (store, fi);
}

static void
delete_response(GtkWidget *selector, guint response, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gchar *name;
	CamelException ex;
	CamelFolder *mail_folder;
        if (response == GTK_RESPONSE_OK) {
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(user_data));
        	if (gtk_tree_selection_get_selected(selection, &model, &iter))
        	{
			gtk_tree_model_get (model, &iter, 1, &name, -1);
			if (gconf_client_get_bool(rss_gconf, GCONF_KEY_REMOVE_FOLDER, NULL))
			{
				//delete folder
				CamelStore *store = mail_component_peek_local_store(NULL);
				gchar *full_path = g_strdup_printf("%s/%s", 
						lookup_main_folder(), 
						lookup_feed_folder(name));
				delete_feed_folder_alloc(lookup_feed_folder(name));
				camel_exception_init (&ex);
				rss_delete_folders (store, full_path, &ex);
				if (camel_exception_is_set (&ex))
				{
                        		e_error_run(NULL,
                                    		"mail:no-delete-folder", full_path, ex.desc, NULL);
                        		camel_exception_clear (&ex);
                		}
				g_free(full_path);
				//also remove status file
				gchar *url =  g_hash_table_lookup(rf->hr, 
							g_hash_table_lookup(rf->hrname, 
							name));
				gchar *buf = gen_md5(url);
				gchar *feed_dir = g_strdup_printf("%s/mail/rss", 
					mail_component_peek_base_directory (mail_component_peek ()));
				gchar *feed_name = g_strdup_printf("%s/%s", feed_dir, buf);
        			g_free(feed_dir);
				g_free(buf);
				unlink(feed_name);
			}
			remove_feed_hash(name);
			g_free(name);
        	}
		gtk_list_store_clear(GTK_LIST_STORE(model));
		g_hash_table_foreach(rf->hrname, construct_list, model);
		save_gconf_feed();
	}
	gtk_widget_destroy(selector);
	rf->import = 0;
}

static void
destroy_delete(GtkWidget *selector, gpointer user_data)
{
	gtk_widget_destroy(user_data);
	rf->import = 0;
}

feeds_dialog_delete(GtkDialog *d, gpointer data)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gchar *name;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data));
	if (gtk_tree_selection_get_selected(selection, &model, &iter) 
		&& !rf->import)
	{
		rf->import = 1;
		gtk_tree_model_get (model, &iter, 1, &name, -1);
		gchar *msg = g_strdup_printf(_("Are you sure you want\n to remove <b>%s</b>?"), name);
		GtkWidget *rfd = remove_feed_dialog(msg);
		gtk_widget_show(rfd);
		g_signal_connect(rfd, "response", G_CALLBACK(delete_response), data);
        	g_signal_connect(rfd, "destroy", G_CALLBACK(destroy_delete), rfd);
		g_free(msg);
		g_free(name);
	}
}

feeds_dialog_disable(GtkDialog *d, gpointer data)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gchar *name;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(rf->treeview));
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get (model, &iter, 1, &name, -1);
		gpointer key = lookup_key(name);
		g_free(name);
		g_hash_table_replace(rf->hre, g_strdup(key), 
			GINT_TO_POINTER(!g_hash_table_lookup(rf->hre, key)));
		gtk_button_set_label(data, 
			g_hash_table_lookup(rf->hre, key) ? _("Disable") : _("Enable"));
	}
	//update list instead of rebuilding
	gtk_list_store_clear(GTK_LIST_STORE(model));
	g_hash_table_foreach(rf->hrname, construct_list, model);
	save_gconf_feed();
}

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
enable_toggle_cb(GtkCellRendererToggle *cell,
               gchar *path_str,
               gpointer data)
{
  GtkTreeModel *model = (GtkTreeModel *)data;
  GtkTreeIter  iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  gchar *name;
  gboolean fixed;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 0, &fixed, -1);
  gtk_tree_model_get (model, &iter, 1, &name, -1);
  fixed ^= 1;
  g_hash_table_replace(rf->hre, 
	g_strdup(lookup_key(name)), 
	GINT_TO_POINTER(fixed));
  gtk_list_store_set (GTK_LIST_STORE (model), 
			&iter, 
			0, 
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
rep_check_cb (GtkWidget *widget, gpointer data)
{
    gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
    /* Save the new setting to gconf */
    gconf_client_set_bool (rss_gconf, GCONF_KEY_REP_CHECK, active, NULL);
    //if we already have a timeout set destroy it first
    if (rf->rc_id && !active)
	g_source_remove(rf->rc_id);
    if (active)
    {
    	//we have to make sure we have a timeout value
    	if (!gconf_client_get_float(rss_gconf, GCONF_KEY_REP_CHECK_TIMEOUT, NULL))
    		gconf_client_set_float (rss_gconf, GCONF_KEY_REP_CHECK_TIMEOUT, 
			gtk_spin_button_get_value((GtkSpinButton *)data), NULL);
    	if (rf->rc_id)
		g_source_remove(rf->rc_id);
    	rf->rc_id = g_timeout_add (60 * 1000 * gtk_spin_button_get_value((GtkSpinButton *)data),
                           (GtkFunction) update_articles,
                           (gpointer)1);
    }
}

static void
set_string_cb (GtkWidget *widget, gpointer data)
{
    const gchar *text = gtk_entry_get_text (GTK_ENTRY (widget));
    gconf_client_set_string (rss_gconf, data, text, NULL);
}

static void
close_details_cb (GtkWidget *widget, gpointer data)
{
	gtk_widget_hide(data);
}

static void
details_cb (GtkWidget *widget, gpointer data)
{
	GtkWidget *details = glade_xml_get_widget(data, "http-proxy-details");
	GtkWidget *close = glade_xml_get_widget(data, "closebutton2");
	GtkWidget *proxy_auth = glade_xml_get_widget(data, "proxy_auth");
	GtkWidget *proxy_user = glade_xml_get_widget(data, "proxy_user");
	GtkWidget *proxy_pass = glade_xml_get_widget(data, "proxy_pass");
	g_signal_connect(close, "clicked", G_CALLBACK(close_details_cb), details);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (proxy_auth),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_AUTH_PROXY, NULL));
	g_signal_connect(proxy_auth, "clicked", G_CALLBACK(start_check_cb), GCONF_KEY_AUTH_PROXY);

	gchar *user = gconf_client_get_string(rss_gconf, GCONF_KEY_USER_PROXY, NULL);
	if (user)
		gtk_entry_set_text(GTK_ENTRY(proxy_user), user);
	g_signal_connect(proxy_user, "changed", G_CALLBACK(set_string_cb), GCONF_KEY_USER_PROXY);
	gchar *pass = gconf_client_get_string(rss_gconf, GCONF_KEY_PASS_PROXY, NULL);
	if (pass)
		gtk_entry_set_text(GTK_ENTRY(proxy_pass), pass);
	g_signal_connect(proxy_pass, "changed", G_CALLBACK(set_string_cb), GCONF_KEY_PASS_PROXY);

	gtk_widget_show(details);
}

static void
host_proxy_cb (GtkWidget *widget, gpointer data)
{
    gconf_client_set_string (rss_gconf, GCONF_KEY_HOST_PROXY, 
		gtk_entry_get_text((GtkEntry*)widget), NULL);
}

static void
port_proxy_cb (GtkWidget *widget, gpointer data)
{
    gconf_client_set_int (rss_gconf, GCONF_KEY_PORT_PROXY, 
		gtk_spin_button_get_value_as_int((GtkSpinButton*)widget), NULL);
}

static void
import_dialog_response(GtkWidget *selector, guint response, gpointer user_data)
{
	while (gtk_events_pending ())
       		gtk_main_iteration ();
        if (response == GTK_RESPONSE_CANCEL)
		rf->cancel = 1;
}

static void
construct_opml_line(gpointer key, gpointer value, gpointer user_data)
{
	gchar *url = g_hash_table_lookup(rf->hr, value);
	gchar *type = g_hash_table_lookup(rf->hrt, value);
	gchar *url_esc = g_markup_escape_text(url, strlen(url));
//	g_free(url);
	gchar *key_esc = g_markup_escape_text(key, strlen(key));
//	g_free(key);
	//g_strdelimit(url_esc, "\"", "'");
	//g_strdelimit(key, "\"", "'");
	gchar *tmp = g_strdup_printf("<outline text=\"%s\" title=\"%s\" type=\"%s\" xmlUrl=\"%s\" htmlUrl=\"%s\"/>\n",
		key_esc, key_esc, type, url_esc, url_esc);
	//gchar *newbuf = g_strescape(tmp, "");
	//gchar *newbuf = g_strescape(tmp, "");
//	g_free(tmp);
//	tmp = newbuf;
	if (buffer != NULL)
		buffer = g_strconcat(buffer, tmp, NULL);
	else
		buffer = g_strdup(tmp);
	g_free(tmp);
	count++;
        float fr = ((count*100)/g_hash_table_size(rf->hr));
        gtk_progress_bar_set_fraction((GtkProgressBar *)user_data, fr/100);
        gchar *what = g_strdup_printf(_("%2.0f%% done"), fr);
        gtk_progress_bar_set_text((GtkProgressBar *)user_data, what);
        g_free(what);
}

void
export_opml(gchar *file)
{
        GtkWidget *import_dialog;
        GtkWidget *import_label;
        GtkWidget *import_progress;
	char outstr[200];
	time_t t;
        struct tm *tmp;
	int btn = GTK_RESPONSE_YES;
	FILE *f;


        gchar *msg = g_strdup(_("Exporting feeds..."));
        import_dialog = e_error_new((GtkWindow *)rf->preferences, "shell:importing", msg, NULL);
	gtk_window_set_keep_above(GTK_WINDOW(import_dialog), TRUE);
//        g_signal_connect(import_dialog, "response", G_CALLBACK(import_dialog_response), NULL);
        import_label = gtk_label_new(_("Please wait"));
        import_progress = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(((GtkDialog *)import_dialog)->vbox), import_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(((GtkDialog *)import_dialog)->vbox), import_progress, FALSE, FALSE, 0);
        gtk_widget_show_all(import_dialog);
        g_free(msg);
	count = 0;
       	g_hash_table_foreach(rf->hrname, construct_opml_line, import_progress);
	gtk_widget_destroy(import_dialog);
        t = time(NULL);
        tmp = localtime(&t);
	strftime(outstr, sizeof(outstr), "%a, %d %b %Y %H:%M:%S %z", tmp);
	gchar *opml = g_strdup_printf("<opml version=\"1.1\">\n<head>\n"
		"<title>Evolution-RSS Exported Feeds</title>\n"
    		"<dateModified>%s</dateModified>\n</head>\n<body>%s</body>\n</opml>\n",
		outstr,
		buffer);
	g_free(buffer);	

	if (g_file_test (file, G_FILE_TEST_IS_REGULAR)) {
                GtkWidget *dlg;

                dlg = gtk_message_dialog_new (GTK_WINDOW (rf->preferences), 0,
                                              GTK_MESSAGE_QUESTION,
                                              GTK_BUTTONS_YES_NO,
                                              _("A file by that name already exists.\n"
                                                "Overwrite it?"));
                gtk_window_set_title (GTK_WINDOW (dlg), _("Overwrite file?"));
                gtk_dialog_set_has_separator (GTK_DIALOG (dlg), FALSE);

                btn = gtk_dialog_run (GTK_DIALOG (dlg));
                gtk_widget_destroy (dlg);
        }

        if (btn == GTK_RESPONSE_YES)
		goto over;
	else
		goto out;

over:	f = fopen(file, "w+");
	if (f)
	{
		fwrite(opml, strlen(opml), 1, f);
		fclose(f);
	}
	else
	{
               	e_error_run(NULL,
			"org-gnome-evolution-rss:feederr",
			_("Error exporting feeds!"),
			g_strerror(errno),
			NULL);
	}
out:	g_free(opml);
	
}

void
import_opml(gchar *file, add_feed *feed)
{
	xmlChar *buff = NULL;
	//some defaults
        feed->changed=0;
        feed->add=1;
	guint total = 0;
	guint current = 0;
	gchar *what = NULL;
	GtkWidget *import_dialog;
	GtkWidget *import_label;
	GtkWidget *import_progress;

	xmlNode *src = (xmlNode *)xmlParseFile (file);
        xmlNode *doc = src;
        gchar *msg = g_strdup(_("Importing feeds..."));
        import_dialog = e_error_new((GtkWindow *)rf->preferences, "shell:importing", msg, NULL);
	gtk_window_set_keep_above(GTK_WINDOW(import_dialog), TRUE);
        g_signal_connect(import_dialog, "response", G_CALLBACK(import_dialog_response), NULL);
        import_label = gtk_label_new(_("Please wait"));
        import_progress = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(((GtkDialog *)import_dialog)->vbox), 
		import_label, 
		FALSE, 
		FALSE, 
		0);
        gtk_box_pack_start(GTK_BOX(((GtkDialog *)import_dialog)->vbox), 
		import_progress, 
		FALSE, 
		FALSE, 
		0);
        gtk_widget_show_all(import_dialog);
	g_free(msg);
        while (src = html_find(src, "outline"))
	{
                feed->feed_url = xmlGetProp((xmlNode *)src, "xmlUrl");
		if (feed->feed_url)
		{
			total++;
			xmlFree(feed->feed_url);
		}
	}
	src = doc;
	//we'll be safer this way
	rf->import = 1;
	while (gtk_events_pending ())
       		gtk_main_iteration ();
        while (src = html_find(src, "outline"))
        {
                feed->feed_url = xmlGetProp((xmlNode *)src, "xmlUrl");
                if (feed->feed_url && strlen(feed->feed_url))
                {
			if (rf->cancel)
			{
				if (src) xmlFree(src);
				rf->cancel = 0;
				goto out;
			}
			gchar *name = xmlGetProp((xmlNode *)src, "title");
			gchar *safe_name = decode_html_entities(name);
			xmlFree(name);
			name = safe_name;
			
			gtk_label_set_text(GTK_LABEL(import_label), name);
#if GTK_2_6
			gtk_label_set_ellipsize (GTK_LABEL (import_label), PANGO_ELLIPSIZE_START);
#endif
			feed->feed_name = name;
			/* we'll get rid of this as soon as we fetch unblocking */
			if (g_hash_table_find(rf->hr,
                                        check_if_match,
                                        feed->feed_url))
                	{
                           rss_error(NULL, feed->feed_name, _("Error adding feed."),
                                           _("Feed already exists!"));
                           continue;
                	}
                	guint res = setup_feed(feed);

			while (gtk_events_pending ())
             			gtk_main_iteration ();
#if RSS_DEBUG
                	g_print("feed imported:%d\n", res);
#endif
			current++;
			float fr = ((current*100)/total);
			gtk_progress_bar_set_fraction((GtkProgressBar *)import_progress, fr/100);
			what = g_strdup_printf(_("%2.0f%% done"), fr);
	        	gtk_progress_bar_set_text((GtkProgressBar *)import_progress, what);
			g_free(what);
			while (gtk_events_pending ())
             			gtk_main_iteration ();
                	GtkTreeModel *model = gtk_tree_view_get_model((GtkTreeView *)rf->treeview);
                	gtk_list_store_clear(GTK_LIST_STORE(model));
                	g_hash_table_foreach(rf->hrname, construct_list, model);
			save_gconf_feed();
			g_free(feed->feed_url);
                	if (src)
                        	xmlFree(src);
                }
                else
                        src = html_find(src, "outline");

        }
	while (gtk_events_pending ())
       		gtk_main_iteration ();
out:    rf->import = 0;
	xmlFree(doc);
        gtk_widget_destroy(import_dialog);
//how the hell should I free this ?
//	g_free(feed);
}

static void
select_file_response(GtkWidget *selector, guint response, gpointer user_data)
{
        if (response == GTK_RESPONSE_OK) {
                char *name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (selector));
		if (name)
		{
        		gtk_widget_hide(selector);
			import_opml(name, user_data);
			g_free(name);
		}
        }
	else
		gtk_widget_destroy(selector);
}

static void
select_export_response(GtkWidget *selector, guint response, gpointer user_data)
{
        if (response == GTK_RESPONSE_OK) {
                char *name;

                name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (selector));
		if (name)
		{
        		gtk_widget_destroy(selector);
			export_opml(name);
			g_free(name);
		}
        }
	else
		gtk_widget_destroy(selector);

}

static void
import_toggle_cb_html (GtkWidget *widget, add_feed *data)
{
        data->fetch_html  = 1-gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void
import_toggle_cb_valid (GtkWidget *widget, add_feed *data)
{
        data->validate  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void
import_toggle_cb_ena (GtkWidget *widget, add_feed *data)
{
        data->enabled  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

GtkWidget*
create_import_dialog (void)
{ 
  GtkWidget *import_file_select;
  GtkWidget *dialog_vbox5;
  GtkWidget *dialog_action_area5;
  GtkWidget *button1;
  GtkWidget *button2;

  import_file_select = gtk_file_chooser_dialog_new (_("Select import file"), NULL, GTK_FILE_CHOOSER_ACTION_OPEN, NULL);
  gtk_window_set_keep_above(GTK_WINDOW(import_file_select), TRUE);
  gtk_window_set_modal (GTK_WINDOW (import_file_select), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (import_file_select), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (import_file_select), GDK_WINDOW_TYPE_HINT_DIALOG);
  
  dialog_vbox5 = GTK_DIALOG (import_file_select)->vbox;
  gtk_widget_show (dialog_vbox5);

  dialog_action_area5 = GTK_DIALOG (import_file_select)->action_area;
  gtk_widget_show (dialog_action_area5);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area5), GTK_BUTTONBOX_END);
  
  button1 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (button1);
  gtk_dialog_add_action_widget (GTK_DIALOG (import_file_select), button1, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (button1, GTK_CAN_DEFAULT);
  
  button2 = gtk_button_new_from_stock ("gtk-open");
  gtk_widget_show (button2);
  gtk_dialog_add_action_widget (GTK_DIALOG (import_file_select), button2, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (button2, GTK_CAN_DEFAULT);

  gtk_widget_grab_default (button2);
  return import_file_select;
}


static void
decorate_import_fs (gpointer data)
{ 
	add_feed *feed = g_new0(add_feed, 1);
        gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (data), TRUE);
        gtk_dialog_set_default_response (GTK_DIALOG (data), GTK_RESPONSE_OK);
        gtk_file_chooser_set_local_only (data, FALSE);

	GtkFileFilter *file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("All Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data), 
					GTK_FILE_FILTER(file_filter));

        file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*.opml");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("OPML Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data), 
					GTK_FILE_FILTER(file_filter));

        file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*.xml");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("XML Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data), 
					GTK_FILE_FILTER(file_filter));

        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (data), 
					GTK_FILE_FILTER(file_filter));


	GtkFileFilter *filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*.opml");
	gtk_file_filter_add_pattern (filter, "*.xml");
	gtk_file_chooser_set_filter(data, filter);

	GtkWidget *vbox1;
	GtkWidget *checkbutton1;
	GtkWidget *checkbutton2;
	GtkWidget *checkbutton3;

	vbox1 = gtk_vbox_new (FALSE, 0);
	checkbutton1 = gtk_check_button_new_with_mnemonic (
                               _("Show article's summary"));
	gtk_widget_show (checkbutton1);
	gtk_box_pack_start (GTK_BOX (vbox1), checkbutton1, FALSE, TRUE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton1), 1);

	checkbutton2 = gtk_check_button_new_with_mnemonic (
                                        _("Feed Enabled"));
	gtk_widget_show (checkbutton2);
	gtk_box_pack_start (GTK_BOX (vbox1), checkbutton2, FALSE, TRUE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton2), 1);

	checkbutton3 = gtk_check_button_new_with_mnemonic (
                                              _("Validate feed"));

	gtk_widget_show (checkbutton3);
	gtk_box_pack_start (GTK_BOX (vbox1), checkbutton3, FALSE, TRUE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton3), 1);

	gtk_file_chooser_set_extra_widget(data, vbox1);
	feed->fetch_html = 0;
	feed->validate = feed->enabled = 1;
	
	g_signal_connect(checkbutton1, 
			"toggled", 
			G_CALLBACK(import_toggle_cb_html), 
			feed);
	g_signal_connect(checkbutton2, 
			"toggled", 
			G_CALLBACK(import_toggle_cb_ena), 
			feed);
	g_signal_connect(checkbutton3, 
			"toggled", 
			G_CALLBACK(import_toggle_cb_valid), 
			feed);
	g_signal_connect(data, "response", G_CALLBACK(select_file_response), feed);
        g_signal_connect(data, "destroy", G_CALLBACK(gtk_widget_destroy), data);
}

GtkWidget*
create_export_dialog (void)
{
  GtkWidget *export_file_select;
  GtkWidget *vbox26;
  GtkWidget *hbuttonbox1;
  GtkWidget *button3;
  GtkWidget *button4;

  export_file_select = gtk_file_chooser_dialog_new (_("Select file to export"), NULL, GTK_FILE_CHOOSER_ACTION_SAVE, NULL);
  gtk_window_set_keep_above(GTK_WINDOW(export_file_select), TRUE);
  g_object_set (export_file_select,
                "local-only", FALSE,
                NULL);
  gtk_window_set_modal (GTK_WINDOW (export_file_select), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (export_file_select), FALSE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (export_file_select), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (export_file_select), GDK_WINDOW_TYPE_HINT_DIALOG);

  vbox26 = GTK_DIALOG (export_file_select)->vbox;
  gtk_widget_show (vbox26);

  hbuttonbox1 = GTK_DIALOG (export_file_select)->action_area;
  gtk_widget_show (hbuttonbox1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);

  button3 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (button3);
  gtk_dialog_add_action_widget (GTK_DIALOG (export_file_select), button3, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (button3, GTK_CAN_DEFAULT);

  button4 = gtk_button_new_from_stock ("gtk-save");
  gtk_widget_show (button4);
  gtk_dialog_add_action_widget (GTK_DIALOG (export_file_select), button4, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (button4, GTK_CAN_DEFAULT);

  gtk_widget_grab_default (button4);
  return export_file_select;
}

static void
decorate_export_fs (gpointer data)
{
        add_feed *feed = g_new0(add_feed, 1);
        gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (data), TRUE);
        gtk_dialog_set_default_response (GTK_DIALOG (data), GTK_RESPONSE_OK);
        gtk_file_chooser_set_local_only (data, FALSE);

        GtkFileFilter *file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("All Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));

        file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*.opml");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("OPML Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));

        file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*.xml");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("XML Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));

        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));


        GtkFileFilter *filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (filter, "*.opml");
        gtk_file_filter_add_pattern (filter, "*.xml");
        gtk_file_chooser_set_filter(data, filter);
	g_signal_connect(data, "response", G_CALLBACK(select_export_response), data);
        g_signal_connect(data, "destroy", G_CALLBACK(gtk_widget_destroy), data);
}

static void
import_cb (GtkWidget *widget, gpointer data)
{
	if (!rf->import)	
	{
		GtkWidget *import = create_import_dialog();
		decorate_import_fs(import);
		gtk_widget_show(import);
	}
	return;
}

static void
export_cb (GtkWidget *widget, gpointer data)
{
	if (!rf->import)
	{
		GtkWidget *export = create_export_dialog();
		decorate_export_fs(export);
        	gtk_dialog_set_default_response (GTK_DIALOG (export), GTK_RESPONSE_OK);
		if (g_hash_table_size(rf->hrname)<1)
        	{
                	e_error_run(NULL,
                	        "org-gnome-evolution-rss:generr",
                	        _("No RSS feeds configured!\nUnable to export."),
                	        NULL);
                	return;
        	}
		gtk_widget_show(export);

//		g_signal_connect(data, "response", G_CALLBACK(select_export_response), data);
//        	g_signal_connect(data, "destroy", G_CALLBACK(gtk_widget_destroy), data);
	}
	return;
}

static void
rep_check_timeout_cb (GtkWidget *widget, gpointer data)
{
    gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data));
    gconf_client_set_float (rss_gconf, GCONF_KEY_REP_CHECK_TIMEOUT, 
		gtk_spin_button_get_value((GtkSpinButton*)widget), NULL);
    if (active)
    {
    	if (rf->rc_id)
		g_source_remove(rf->rc_id);
    	rf->rc_id = g_timeout_add (60 * 1000 * gtk_spin_button_get_value((GtkSpinButton *)widget),
                           (GtkFunction) update_articles,
                           (gpointer)1);
    }
}

static void
del_days_cb (GtkWidget *widget, add_feed *data)
{
	guint adj = gtk_spin_button_get_value((GtkSpinButton*)widget);
	data->del_days = adj;
}

static void
del_messages_cb (GtkWidget *widget, add_feed *data)
{
	guint adj = gtk_spin_button_get_value((GtkSpinButton*)widget);
	data->del_messages = adj;
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
	SoupUri *newuri;
        gchar *newuristr;
        SoupUri *base_uri = soup_uri_new (base);
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
        				SoupUri *newbase_uri = soup_uri_new (basehref);
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
//	g_print("value size:%d\n", a->value);
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
		g_print("data:%p\n", data);
		g_print("is_widget:%d\n", GTK_IS_WIDGET(widget));
		g_print("is_data:%p\n", data);
		g_print("is_is_data:%d\n", GTK_IS_WIDGET(data));
//		if (GTK_IS_MOZ_EMBED(data))
//		{
//			g_print("is mozembed\n");
       		if (data && GTK_IS_WIDGET(data) && height > 50)
		{
			gtk_widget_set_size_request((GtkWidget *)data, width, height);
// apparently resizing gtkmozembed widget won't redraw if using xulrunner
// there is no point in reload for the rest
#ifdef HAVE_XULRUNNER
			gtk_moz_embed_reload(rf->mozembed, GTK_MOZ_EMBED_FLAG_RELOADNORMAL);
#endif
		}
//		}
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
	GtkWidget *gpage;
	GtkWidget *gpage2;
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
		gpage = (GtkWidget *)webkit_gtk_page_new();
		gtk_container_add(GTK_CONTAINER(moz), GTK_WIDGET(gpage));
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
        		webkit_gtk_page_open(WEBKIT_GTK_PAGE(gpage), po->website);
		else
        		webkit_gtk_page_open(WEBKIT_GTK_PAGE(gpage), "about:blank");
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
//	gtk_box_pack_start (GTK_BOX (w), gpage, TRUE, TRUE, 0);
	gtk_widget_show_all(moz);
        gtk_container_add ((GtkContainer *) eb, moz);
	g_print("add\n");
//	gtk_widget_set_size_request((GtkWidget *)rf->mozembed, 330, 330);
//        gtk_container_add ((GtkContainer *) eb, rf->mozembed);
	EMFormat *myf = (EMFormat *)efh;
	rf->headers_mode = myf->mode;
	g_signal_connect(efh->html,
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
	GtkRequisition req;
	gtk_widget_size_request(eb, &req);
	g_print("ww:%d,hh%d\n", req.width, req.height);

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
#ifdef HAVE_GTKMOZEMBED
		gtk_moz_embed_stop_load(GTK_MOZ_EMBED(rf->mozembed));
//		gtk_moz_embed_pop_startup();
#endif
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
        g_print("formatting\n");
        g_print("html\n");

	g_print("RENDER:%s\n", RENDER);
	g_print("RENDER:%d\n", RENDER_N);

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
		xmlDoc *doc = parse_html(addr, content->str, content->len);
		if (doc)
		{
			htmlDocDumpMemory(doc, &buff, &size);
#ifdef RSS_DEBUG
			g_print("%s\n", buff);
#endif
			xmlFree(doc);
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
		g_print("normal html rendering\n");
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
	g_print("out\n");
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

void org_gnome_cooly_article_show(void *ep, EMPopupTargetSelect *t);

void org_gnome_cooly_article_show(void *ep, EMPopupTargetSelect *t)
{
	g_print("(l)user is reading mail\n");
}

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
	g_print("here\n");
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
finish_feed (SoupMessage *msg, gpointer user_data)
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
	
	if (!msg->response.length)
		goto out;

	if (msg->status_code == SOUP_STATUS_CANCELLED)
		goto out;



	GString *response = g_string_new_len(msg->response.body, msg->response.length);
//#ifdef RSS_DEBUG
	g_print("feed %s\n", user_data);
//#endif

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
		g_hash_table_insert(rf->activity, key, (gpointer)activity_id);

		net_get_unblocking(
				g_hash_table_lookup(rf->hr, lookup_key(key)),
				user_data,
				key,
				(gpointer)finish_feed,
				g_strdup(key),		// we need to dupe key here
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

void
delete_feed_folder_alloc(gchar *old_name)
{
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

        gchar *orig_name = g_hash_table_lookup(rf->feed_folders, old_name);
        if (orig_name)
                g_hash_table_remove(rf->feed_folders, old_name);

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
#if GTK_2_6
	gtk_label_set_ellipsize (GTK_LABEL (label2), PANGO_ELLIPSIZE_START);
#endif
	gtk_label_set_justify(GTK_LABEL(label2), GTK_JUSTIFY_FILL);
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
#if GTK_2_6
        gtk_label_set_ellipsize (
                GTK_LABEL (label), PANGO_ELLIPSIZE_END);
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
			g_print("init_dbus()\n");
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

	camel_folder_append_message(mail_folder, new, info, NULL, NULL);
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
finish_enclosure (SoupMessage *msg, create_feed *user_data)
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
		fwrite(msg->response.body, msg->response.length, 1, f);
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
					     XML_SUBSTITUTE_REF,
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
               	if (info) {
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
                	if (info) {
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

void
check_feed_age(void)
{
//	g_hash_table_foreach(rf->hrname, get_feed_age);	
}

/*static void
rdf_free (RDF *r)
{
	/* Stop the download */
/*	if (r->message) {
		soup_message_cancel (r->message);
	}

	g_free (r->uri);
	g_free (r->html);

	if (r->cache) {
		xmlFreeDoc (r->cache);
	}

	g_free (r);
}

static void
e_summary_rdf_set_online (ESummary *summary,
			  GNOME_Evolution_OfflineProgressListener progress,
			  gboolean online,
			  void *data)
{
	ESummaryRDF *rdf;
	GList *p;

	rdf = summary->rdf;
	if (rdf->online == online) {
		return;
	}

	if (online == TRUE) {
		e_summary_rdf_update (summary);

		if (summary->preferences->rdf_refresh_time != 0)
			rdf->timeout = gtk_timeout_add (summary->preferences->rdf_refresh_time * 1000,
							(GtkFunction) e_summary_rdf_update,
							summary);
	} else {
		for (p = rdf->rdfs; p; p = p->next) {
			RDF *r;

			r = p->data;
			if (r->message) {
				soup_message_cancel (r->message);
				r->message = NULL;
			}
		}

		gtk_timeout_remove (rdf->timeout);
		rdf->timeout = 0;
	}

	rdf->online = online;
}

void
e_summary_rdf_init (ESummary *summary)
{
	ESummaryPrefs *prefs;
	ESummaryRDF *rdf;
	ESummaryConnection *connection;
	GSList *p;
	int timeout;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	prefs = summary->preferences;
	g_assert (prefs != NULL);

	rdf = g_new0 (ESummaryRDF, 1);
	summary->rdf = rdf;

	connection = g_new (ESummaryConnection, 1);
	connection->count = e_summary_rdf_count;
	connection->add = e_summary_rdf_add;
	connection->set_online = e_summary_rdf_set_online;
	connection->closure = NULL;
	connection->callback = NULL;
	connection->callback_closure = NULL;

	rdf->connection = connection;
	rdf->online = TRUE;
	e_summary_add_online_connection (summary, connection);

	e_summary_add_protocol_listener (summary, "rdf", e_summary_rdf_protocol, rdf);

	for (p = prefs->rdf_urls; p; p = p->next) {
		e_summary_rdf_add_uri (summary, p->data);
	}
	timeout = prefs->rdf_refresh_time;

	e_summary_rdf_update (summary);

	if (rdf->timeout == 0)
		rdf->timeout = 0;
	else
		rdf->timeout = gtk_timeout_add (timeout * 1000,
						(GtkFunction) e_summary_rdf_update, summary);

	return;
}

void
e_summary_rdf_reconfigure (ESummary *summary)
{
	ESummaryRDF *rdf;
	GList *old, *p;
	GSList *sp;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	rdf = summary->rdf;

	/* Stop timeout */
/*	if (rdf->timeout != 0) {
		gtk_timeout_remove (rdf->timeout);
		rdf->timeout = 0;
	}

	old = rdf->rdfs;
	rdf->rdfs = NULL;
	for (p = old; p; p = p->next) {
		RDF *r;

		r = p->data;
		rdf_free (r);
	}
	g_list_free (old);

	for (sp = summary->preferences->rdf_urls; sp; sp = sp->next) {
		e_summary_rdf_add_uri (summary, sp->data);
	}

	if (summary->preferences->rdf_refresh_time != 0)
		rdf->timeout = gtk_timeout_add (summary->preferences->rdf_refresh_time * 1000,
						(GtkFunction) e_summary_rdf_update, summary);

	e_summary_rdf_update (summary);
}

void
e_summary_rdf_free (ESummary *summary)
{
	ESummaryRDF *rdf;
	GList *p;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	rdf = summary->rdf;

	if (rdf->timeout != 0)
		gtk_timeout_remove (rdf->timeout);

	for (p = rdf->rdfs; p; p = p->next) {
		RDF *r = p->data;

		rdf_free (r);
	}
	g_list_free (rdf->rdfs);
	g_free (rdf->html);

	e_summary_remove_online_connection (summary, rdf->connection);
	g_free (rdf->connection);

	g_free (rdf);
	summary->rdf = NULL;
}
*/

/*=============*
 * BONOBO part *
 *=============*/

static void
proxy_toggled_cb (GtkToggleButton *toggle, gpointer data)
{
        if (!GTK_IS_RADIO_BUTTON (toggle) || toggle->active)
		g_print("toggle\n");
}

static void
set_sensitive (GtkCellLayout   *cell_layout,
	       GtkCellRenderer *cell,
	       GtkTreeModel    *tree_model,
	       GtkTreeIter     *iter,
	       gpointer         data)
{
  GtkTreePath *path;
  gint *indices;
  gboolean sensitive = 1;

  path = gtk_tree_model_get_path (tree_model, iter);
  indices = gtk_tree_path_get_indices (path);

  if (indices[0] == 1)
#ifdef HAVE_WEBKIT
	sensitive = 1;
#else
	sensitive = 0;
#endif

  if (indices[0] == 2)
#ifdef HAVE_GTKMOZEMBED
	sensitive = 1;
#else
	sensitive = 0;
#endif


  gtk_tree_path_free (path);

  g_object_set (cell, "sensitive", sensitive, NULL);
}

static struct {
        const char *label;
        const int key;
} engines[] = {
        { N_("GtkHTML"), 0 },
        { N_("WebKit"), 1 },
        { N_("Mozilla"), 2 },
};

static void
render_engine_changed (GtkComboBox *dropdown, GCallback *user_data)
{
        int id = gtk_combo_box_get_active (dropdown);
        GtkTreeModel *model;
        GtkTreeIter iter;

        model = gtk_combo_box_get_model (dropdown);
        if (id == -1 || !gtk_tree_model_iter_nth_child (model, &iter, NULL, id))
                return;
	if (!id) id = 10;
	gconf_client_set_int(rss_gconf, GCONF_KEY_HTML_RENDER, id, NULL);
#ifdef HAVE_GTKMOZEMBED
	if (id == 2)
		rss_mozilla_init();
#endif
}

EvolutionConfigControl*
rss_config_control_new (void)
{
        GtkWidget *control_widget;
        char *gladefile;
	setupfeed *sf;
	
	GtkListStore  *store;
	GtkTreeIter    iter;
	int i;
	GtkCellRenderer *cell;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;

	g_print("rf->%p\n", rf);
	sf = g_new0(setupfeed, 1);

        gladefile = g_build_filename (EVOLUTION_GLADEDIR,
                                      "rss-ui.glade",
                                      NULL);
        sf->gui = glade_xml_new (gladefile, NULL, NULL);
        g_free (gladefile);

        GtkTreeView *treeview = (GtkTreeView *)glade_xml_get_widget (sf->gui, "feeds-treeview");
	rf->treeview = (GtkWidget *)treeview;
	sf->treeview = (GtkWidget *)treeview;

	//gtk_widget_set_size_request ((GtkWidget *)treeview, 395, -1);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

	store = gtk_list_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING,
                                G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), (GtkTreeModel *)store);

	cell = gtk_cell_renderer_toggle_new ();

	column = gtk_tree_view_column_new_with_attributes (_("Enabled"),
                                                  cell,
                                                  "active", 0,
                                                  NULL);
	g_signal_connect((gpointer) cell, "toggled", G_CALLBACK(enable_toggle_cb), store);
	gtk_tree_view_column_set_resizable(column, FALSE);
	gtk_tree_view_column_set_max_width (column, 70);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview),
                               column);
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Feed Name"),
                                                  cell,
                                                  "text", 1,
                                                  NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview),
                               column);
	gtk_tree_view_column_set_sort_column_id (column, 1);
	gtk_tree_view_column_clicked(column);
	column = gtk_tree_view_column_new_with_attributes (_("Type"),
                                                  cell,
                                                  "text", 2,
                                                  NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview),
                               column);
	gtk_tree_view_column_set_sort_column_id (column, 2);
	gtk_tree_view_set_search_column (GTK_TREE_VIEW (treeview),
                                                   2);

	if (rf->hr != NULL)
        	g_hash_table_foreach(rf->hrname, construct_list, store);

	//make sure something (first row) is selected
       selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
       gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, 0);
       gtk_tree_selection_select_iter(selection, &iter);
 
	g_signal_connect((gpointer) treeview, 
			"row_activated", 
			G_CALLBACK(treeview_row_activated), 
			treeview);

       GtkWidget *button1 = glade_xml_get_widget (sf->gui, "feed-add-button");
       g_signal_connect(button1, "clicked", G_CALLBACK(feeds_dialog_add), treeview);

       GtkWidget *button2 = glade_xml_get_widget (sf->gui, "feed-edit-button");
       g_signal_connect(button2, "clicked", G_CALLBACK(feeds_dialog_edit), treeview);

       GtkWidget *button3 = glade_xml_get_widget (sf->gui, "feed-delete-button");
       g_signal_connect(button3, "clicked", G_CALLBACK(feeds_dialog_delete), treeview);


	rf->preferences = glade_xml_get_widget (sf->gui, "rss-config-control");
	sf->add_feed = glade_xml_get_widget (sf->gui, "add-feed-dialog");
	sf->check1 = glade_xml_get_widget(sf->gui, "checkbutton1");
	sf->check2 = glade_xml_get_widget(sf->gui, "checkbutton2");
	sf->check3 = glade_xml_get_widget(sf->gui, "checkbutton3");
	sf->spin = glade_xml_get_widget(sf->gui, "spinbutton1");

 	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->check1), 
		gconf_client_get_bool(rss_gconf, GCONF_KEY_REP_CHECK, NULL));

  	gdouble adj = gconf_client_get_float(rss_gconf, GCONF_KEY_REP_CHECK_TIMEOUT, NULL);
  	if (adj)
		gtk_spin_button_set_value((GtkSpinButton *)sf->spin, adj);
	g_signal_connect(sf->check1, "clicked", G_CALLBACK(rep_check_cb), sf->spin);
	g_signal_connect(sf->spin, "changed", G_CALLBACK(rep_check_timeout_cb), sf->check1);
	g_signal_connect(sf->spin, "value-changed", G_CALLBACK(rep_check_timeout_cb), sf->check1);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->check2),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_START_CHECK, NULL));
	g_signal_connect(sf->check2, 
		"clicked", 
		G_CALLBACK(start_check_cb), 
		GCONF_KEY_START_CHECK);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->check3),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_DISPLAY_SUMMARY, NULL));
	g_signal_connect(sf->check3, 
		"clicked", 
		G_CALLBACK(start_check_cb), 
		GCONF_KEY_DISPLAY_SUMMARY);


	/* HTML tab */
	sf->combo_hbox = glade_xml_get_widget(sf->gui, "hbox17");
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
        store = gtk_list_store_new(1, G_TYPE_STRING);
	GtkWidget *combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
        for (i=0;i<3;i++) {
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter, 0, _(engines[i].label), -1);
        }
   	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
				    "text", 0,
				    NULL);
	guint render = GPOINTER_TO_INT(gconf_client_get_int(rss_gconf,
                                    GCONF_KEY_HTML_RENDER,
                                    NULL));

	switch (render)
	{
		case 10:
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			break;
		case 1: 
#ifndef HAVE_WEBKIT
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			break;
#endif
		case 2:
#ifndef HAVE_GTKMOZEMBED
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			break;
#endif
		default:
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), render);

	}

	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo),
					renderer,
					set_sensitive,
					NULL, NULL);

#if !defined(HAVE_GTKMOZEMBED) && !defined (HAVE_WEBKIT)
	GtkWidget *label_webkit = glade_xml_get_widget(sf->gui, "label_webkits");
	gtk_label_set_text(label_webkit, _("Note: In order to be able to use Mozilla (Firefox) or Apple Webkit \nas renders you need firefox or webkit devel package \ninstalled and evolution-rss should be recompiled to see those packages."));
	gtk_widget_show(label_webkit);
#endif
	g_signal_connect (combo, "changed", G_CALLBACK (render_engine_changed), NULL);
	g_signal_connect (combo, "value-changed", G_CALLBACK (render_engine_changed), NULL);
	gtk_widget_show(combo);
	gtk_box_pack_start(GTK_BOX(sf->combo_hbox), combo, FALSE, FALSE, 0);

	/* Network tab */
	sf->use_proxy = glade_xml_get_widget(sf->gui, "use_proxy");
	sf->host_proxy = glade_xml_get_widget(sf->gui, "host_proxy");
	sf->port_proxy = glade_xml_get_widget(sf->gui, "port_proxy");
	sf->details = glade_xml_get_widget(sf->gui, "details");
	sf->proxy_details = glade_xml_get_widget(sf->gui, "http-proxy-details");

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->use_proxy),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_USE_PROXY, NULL));
	g_signal_connect(sf->use_proxy, "clicked", G_CALLBACK(start_check_cb), GCONF_KEY_USE_PROXY);

	gchar *host = gconf_client_get_string(rss_gconf, GCONF_KEY_HOST_PROXY, NULL);
	if (host)
		gtk_entry_set_text(GTK_ENTRY(sf->host_proxy), host);
	g_signal_connect(sf->host_proxy, "changed", G_CALLBACK(host_proxy_cb), NULL);

  	gint port = gconf_client_get_int(rss_gconf, GCONF_KEY_PORT_PROXY, NULL);
  	if (port)
		gtk_spin_button_set_value((GtkSpinButton *)sf->port_proxy, (gdouble)port);
	g_signal_connect(sf->port_proxy, "changed", G_CALLBACK(port_proxy_cb), NULL);
	g_signal_connect(sf->port_proxy, "value_changed", G_CALLBACK(port_proxy_cb), NULL);

	g_signal_connect(sf->details, "clicked", G_CALLBACK(details_cb), sf->gui);


	sf->import = glade_xml_get_widget(sf->gui, "import");
	sf->export = glade_xml_get_widget(sf->gui, "export");
	g_signal_connect(sf->import, "clicked", G_CALLBACK(import_cb), sf->import);
	g_signal_connect(sf->export, "clicked", G_CALLBACK(export_cb), sf->export);

        control_widget = glade_xml_get_widget (sf->gui, "feeds-notebook");
        gtk_widget_ref (control_widget);

        gtk_container_remove (GTK_CONTAINER (control_widget->parent), control_widget);

        return evolution_config_control_new (control_widget);
}


static BonoboObject *
factory (BonoboGenericFactory *factory,
         const char *component_id,
         void *closure)
{
	g_return_val_if_fail(upgrade == 2, NULL);

	g_print("component_id:%s\n", component_id);

        if (strcmp (component_id, RSS_CONTROL_ID) == 0)
                return BONOBO_OBJECT (rss_config_control_new ());

        g_warning (FACTORY_ID ": Don't know what to do with %s", component_id);
        return NULL;
}


BONOBO_ACTIVATION_SHLIB_FACTORY (FACTORY_ID, "Evolution RSS component factory", factory, NULL)
