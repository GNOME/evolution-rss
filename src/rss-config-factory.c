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

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <gdk/gdkkeysyms.h>

#include <camel/camel-store.h>
#include <camel/camel-provider.h>

#include <mail/em-config.h>

#include <shell/evolution-config-control.h>
#include <e-util/e-error.h>
#include <bonobo/bonobo-shlib-factory.h>

#ifdef HAVE_LIBSOUP_GNOME
#include <libsoup/soup-gnome.h>
#include <libsoup/soup-gnome-features.h>
#endif

#include "rss.h"
#include "misc.h"
#include "parser.h"
#include "rss-config-factory.h"
#include "network-soup.h"

#define d(x)

static guint feed_enabled = 0;
static guint feed_validate = 0;
static guint feed_html = 0;
guint ccurrent = 0, ctotal = 0;

extern rssfeed *rf;
extern guint upgrade;
extern guint count; 
extern gchar *buffer;
extern GSList *rss_list;
extern GConfClient *rss_gconf;
extern GHashTable *icons;
extern SoupCookieJar *rss_soup_jar;

#define RSS_CONTROL_ID  "OAFIID:GNOME_Evolution_RSS:" EVOLUTION_VERSION_STRING
#define FACTORY_ID      "OAFIID:GNOME_Evolution_RSS_Factory:" EVOLUTION_VERSION_STRING
#define MAX_TTL		10000

typedef struct {
        GladeXML *xml;
        GConfClient *gconf;
        GtkWidget   *combobox;
        GtkWidget   *check1;
        GtkWidget   *check2;
        GtkWidget   *nettimeout;
        GtkWidget   *check3;
        GtkWidget   *check4;
        GtkWidget   *check5;
        GtkWidget   *check6;
        GtkWidget   *import;
} UIData;

typedef struct _setupfeed {
        GladeXML  *gui;
        GtkWidget *treeview;
        GtkWidget *add_feed;
        GtkWidget *check1;
        GtkWidget *check2;
        GtkWidget *check3;
        GtkWidget *check4;
        GtkWidget *spin;
        GtkWidget *use_proxy;
        GtkWidget *host_proxy;
        GtkWidget *port_proxy;
        GtkWidget *proxy_details;
        GtkWidget *details;
        GtkWidget *import;
        GtkWidget *import_fs;
        GtkWidget *export_fs;
        GtkWidget *export;
        GtkWidget *combo_hbox;
} setupfeed;

static void feeds_dialog_edit(GtkDialog *d, gpointer data);
void decorate_import_cookies_fs (gpointer data);
gboolean store_redrawing = FALSE;

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
#ifdef HAVE_GECKO
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
#ifdef HAVE_GECKO
        if (id == 2)
                rss_mozilla_init();
#endif
}

static void
start_check_cb(GtkWidget *widget, gpointer data)
{
    gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
    /* Save the new setting to gconf */
    gconf_client_set_bool (rss_gconf, data, active, NULL);
}

void
accept_cookies_cb(GtkWidget *widget, GtkWidget *data)
{
	gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	/* Save the new setting to gconf */
	gconf_client_set_bool (rss_gconf, GCONF_KEY_ACCEPT_COOKIES, active, NULL);
	gtk_widget_set_sensitive(data, active);
	
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
  gtk_tree_model_get (model, &iter, 3, &name, -1);
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
treeview_row_activated(GtkTreeView *treeview,
                       GtkTreePath *path, GtkTreeViewColumn *column)
{
        feeds_dialog_edit((GtkDialog *)treeview, treeview);
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
         if (active) {
	     gtk_spin_button_update((GtkSpinButton *)data);
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
rep_check_timeout_cb (GtkWidget *widget, gpointer data)
{
    gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data));
    gtk_spin_button_update((GtkSpinButton *)widget);
    gconf_client_set_float (rss_gconf, GCONF_KEY_REP_CHECK_TIMEOUT,
                gtk_spin_button_get_value((GtkSpinButton*)widget), NULL);
    if (active) {
        if (rf->rc_id)
                g_source_remove(rf->rc_id);
        rf->rc_id = g_timeout_add (60 * 1000 * gtk_spin_button_get_value((GtkSpinButton *)widget),
                           (GtkFunction) update_articles,
                           (gpointer)1);
    }
}

static void
close_details_cb (GtkWidget *widget, gpointer data)
{
        gtk_widget_hide(data);
}

static void
set_string_cb (GtkWidget *widget, gpointer data)
{
    const gchar *text = gtk_entry_get_text (GTK_ENTRY (widget));
    gconf_client_set_string (rss_gconf, data, text, NULL);
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
construct_list(gpointer key, gpointer value, gpointer user_data)
{
        GtkListStore  *store = user_data;
        GtkTreeIter    iter;

        gtk_list_store_append (store, &iter);
	gchar *full_name = lookup_feed_folder(key);
	gchar *name = g_path_get_basename(full_name);
	gchar *full_path = g_strconcat(lookup_main_folder(), "/", full_name, NULL);
        gtk_list_store_set (store, &iter,
                0, g_hash_table_lookup(rf->hre, lookup_key(key)),
                1, name,
                2, g_hash_table_lookup(rf->hrt, lookup_key(key)),
		3, g_strdup(key),
		4, full_path,
                -1);
	g_free(name);
	g_free(full_path);
}

static void
ttl_multiply_cb (GtkWidget *widget, add_feed *data)
{
        guint active = gtk_combo_box_get_active((GtkComboBox*)widget);
	data->ttl_multiply = active;
}

static void
ttl_cb (GtkWidget *widget, add_feed *data)
{
        guint adj = gtk_spin_button_get_value((GtkSpinButton*)widget);
        data->ttl = adj;
}

void
del_days_cb (GtkWidget *widget, add_feed *data)
{
        guint adj = gtk_spin_button_get_value((GtkSpinButton*)widget);
        data->del_days = adj;
}

void
del_messages_cb (GtkWidget *widget, add_feed *data)
{
        guint adj = gtk_spin_button_get_value((GtkSpinButton*)widget);
        data->del_messages = adj;
}

void
disable_widget_cb(GtkWidget *widget, GladeXML *data)
{
	GtkWidget *authuser = (GtkWidget *)glade_xml_get_widget (data, "auth_user");
	GtkWidget *authpass = (GtkWidget *)glade_xml_get_widget (data, "auth_pass");
	GtkToggleButton *useauth = (GtkToggleButton *)glade_xml_get_widget (data, "use_auth");
	gboolean auth_enabled = gtk_toggle_button_get_active(useauth);

	gtk_widget_set_sensitive(authuser, auth_enabled);
	gtk_widget_set_sensitive(authpass, auth_enabled);
}

add_feed *
build_dialog_add(gchar *url, gchar *feed_text)
{
        char *gladefile;
  	add_feed *feed = g_new0(add_feed, 1);
  	feed->enabled = TRUE;
	GladeXML  *gui;
	gchar *flabel = NULL;
  	gboolean fhtml = FALSE;
  	gboolean del_unread = FALSE;
  	guint del_feed = 0;
	gpointer key = NULL;
	GtkAccelGroup *accel_group = gtk_accel_group_new ();

        gladefile = g_build_filename (EVOLUTION_GLADEDIR,
                                      "rss-ui.glade",
                                      NULL);
        gui = glade_xml_new (gladefile, NULL, GETTEXT_PACKAGE);
        g_free (gladefile);

        GtkWidget *dialog1 = (GtkWidget *)glade_xml_get_widget (gui, "feed_dialog");
        GtkWidget *child = (GtkWidget *)glade_xml_get_widget (gui, "dialog-vbox9");
//	gtk_widget_show(dialog1);
//  	gtk_window_set_keep_above(GTK_WINDOW(dialog1), FALSE);
 	if (url != NULL)
        	gtk_window_set_title (GTK_WINDOW (dialog1), _("Edit Feed"));
  	else
        	gtk_window_set_title (GTK_WINDOW (dialog1), _("Add Feed"));
//  	gtk_window_set_modal (GTK_WINDOW (dialog1), FALSE);

	
        GtkWidget *adv_options = (GtkWidget *)glade_xml_get_widget (gui, "adv_options");

        GtkWidget *entry1 = (GtkWidget *)glade_xml_get_widget (gui, "url_entry");
  	//editing
  	if (url != NULL) {
  		key = lookup_key(feed_text);
		gtk_expander_set_expanded(GTK_EXPANDER(adv_options), TRUE);	
  		gtk_entry_set_text(GTK_ENTRY(entry1), url);
		fhtml = GPOINTER_TO_INT(
  	              	g_hash_table_lookup(rf->hrh, key));
       	 	feed->enabled = GPOINTER_TO_INT(
       	         	g_hash_table_lookup(rf->hre, key));
        	del_feed = GPOINTER_TO_INT(
                	g_hash_table_lookup(rf->hrdel_feed, key));
        	del_unread = GPOINTER_TO_INT(
                	g_hash_table_lookup(rf->hrdel_unread, key));
        	feed->del_days = GPOINTER_TO_INT(
                	g_hash_table_lookup(rf->hrdel_days, key));
        	feed->del_messages = GPOINTER_TO_INT(
                	g_hash_table_lookup(rf->hrdel_messages, key));
        	feed->update = GPOINTER_TO_INT(
                	g_hash_table_lookup(rf->hrupdate, key));
        	feed->ttl = GPOINTER_TO_INT(
                	g_hash_table_lookup(rf->hrttl, key));
        	feed->ttl_multiply = GPOINTER_TO_INT(
                	g_hash_table_lookup(rf->hrttl_multiply, key));
  	}
  	feed->validate = 1;

        GtkWidget *entry2 = (GtkWidget *)glade_xml_get_widget (gui, "entry2");
        GtkWidget *feed_name = (GtkWidget *)glade_xml_get_widget (gui, "feed_name");
	if (url != NULL) {
		flabel = g_build_path("/", 
				lookup_main_folder(),
				lookup_feed_folder(feed_text),
				NULL);
		gtk_label_set_text(GTK_LABEL(entry2), flabel);
		gchar *fname = g_path_get_basename(lookup_feed_folder(feed_text));
		gtk_entry_set_text(GTK_ENTRY(feed_name), fname);
		g_free(fname);
		gtk_widget_show(feed_name);
        	GtkWidget *feed_name_label = (GtkWidget *)glade_xml_get_widget (gui, "feed_name_label");
		gtk_widget_show(feed_name_label);
        	GtkWidget *location_button = (GtkWidget *)glade_xml_get_widget (gui, "location_button");
		gtk_widget_show(location_button);
        	GtkWidget *location_label = (GtkWidget *)glade_xml_get_widget (gui, "location_label");
		gtk_widget_show(location_label);
        	gtk_label_set_use_markup(GTK_LABEL(entry2), 1);
  	} else
		gtk_label_set_text(GTK_LABEL(entry2), flabel);

	GtkWidget *combobox1 = (GtkWidget *)glade_xml_get_widget (gui, "combobox1");
	gtk_combo_box_set_active(GTK_COMBO_BOX(combobox1), 0);

	GtkWidget *checkbutton1 = (GtkWidget *)glade_xml_get_widget (gui, "html_check");
  	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton1), 1-fhtml);

	GtkWidget *checkbutton2 = (GtkWidget *)glade_xml_get_widget (gui, "enabled_check");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton2), feed->enabled);

	GtkWidget *checkbutton3 = (GtkWidget *)glade_xml_get_widget (gui, "validate_check");
	if (url)
        	gtk_widget_set_sensitive(checkbutton3, FALSE);
  	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton3), feed->validate);

	GtkWidget *spinbutton1 = (GtkWidget *)glade_xml_get_widget (gui, "storage_sb1");
	GtkWidget *spinbutton2 = (GtkWidget *)glade_xml_get_widget (gui, "storage_sb2");
  	if (feed->del_messages)
        	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton1), feed->del_messages);
	g_signal_connect(spinbutton1, "changed", G_CALLBACK(del_messages_cb), feed);

	GtkWidget *radiobutton1 = (GtkWidget *)glade_xml_get_widget (gui, "storage_rb1");
	GtkWidget *radiobutton2 = (GtkWidget *)glade_xml_get_widget (gui, "storage_rb2");
	GtkWidget *radiobutton3 = (GtkWidget *)glade_xml_get_widget (gui, "storage_rb3");
	GtkWidget *radiobutton7 = (GtkWidget *)glade_xml_get_widget (gui, "storage_rb4");
	GtkWidget *radiobutton4 = (GtkWidget *)glade_xml_get_widget (gui, "ttl_global");
	GtkWidget *radiobutton5 = (GtkWidget *)glade_xml_get_widget (gui, "ttl");
	GtkWidget *radiobutton6 = (GtkWidget *)glade_xml_get_widget (gui, "ttl_disabled");
	GtkWidget *ttl_value = (GtkWidget *)glade_xml_get_widget (gui, "ttl_value");
        GtkImage *image = (GtkImage *)glade_xml_get_widget (gui, "image1");
	gtk_spin_button_set_range((GtkSpinButton *)ttl_value, 0, (guint)MAX_TTL);

	/*set feed icon*/
	if (key) {
		gtk_image_set_from_icon_name(image, 
			g_hash_table_lookup(icons, key) ? key : "evolution-rss-main",
			GTK_ICON_SIZE_SMALL_TOOLBAR);
		gtk_widget_show(GTK_WIDGET(image));
	}

  	switch (del_feed) {
        case 1:         //all but the last
                gtk_toggle_button_set_active(
                        GTK_TOGGLE_BUTTON(radiobutton2), 1);
                break;
        case 2:         //older than days
                gtk_toggle_button_set_active(
                        GTK_TOGGLE_BUTTON(radiobutton3), 1);
                break;
        case 3:         //articles not present in feed
                gtk_toggle_button_set_active(
                        GTK_TOGGLE_BUTTON(radiobutton7), 1);
                break;
        default:
                gtk_toggle_button_set_active(
                        GTK_TOGGLE_BUTTON(radiobutton1), 1);
  	}

  	if (feed->del_days)
        	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton2), feed->del_days);
	g_signal_connect(spinbutton2, "changed", G_CALLBACK(del_days_cb), feed);

	GtkWidget *checkbutton4 = (GtkWidget *)glade_xml_get_widget (gui, "storage_unread");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton4), del_unread);

       	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ttl_value), feed->ttl);
	g_signal_connect(ttl_value, "changed", G_CALLBACK(ttl_cb), feed);

       	gtk_combo_box_set_active(GTK_COMBO_BOX(combobox1), feed->ttl_multiply);
	g_signal_connect(combobox1, "changed", G_CALLBACK(ttl_multiply_cb), feed);

	switch (feed->update) {
	case 2:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton5), 1);
		break;
	case 3:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton6), 1);
		break;
	default:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton4), 1);
		break;
	}

	GtkWidget *authuser = (GtkWidget *)glade_xml_get_widget (gui, "auth_user");
	GtkWidget *authpass = (GtkWidget *)glade_xml_get_widget (gui, "auth_pass");
	GtkToggleButton *useauth = (GtkToggleButton *)glade_xml_get_widget (gui, "use_auth");

	if (url && read_up(url)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (useauth), 1);
		gtk_entry_set_text(GTK_ENTRY(authuser), g_hash_table_lookup(rf->hruser, url));
		gtk_entry_set_text(GTK_ENTRY(authpass), g_hash_table_lookup(rf->hrpass, url));
	}
	gboolean auth_enabled = gtk_toggle_button_get_active(useauth);
	gtk_widget_set_sensitive(authuser, auth_enabled);
	gtk_widget_set_sensitive(authpass, auth_enabled);
	g_signal_connect(useauth, "toggled", G_CALLBACK(disable_widget_cb), gui);

	GtkWidget *ok = (GtkWidget *)glade_xml_get_widget (gui, "ok_button");
	/*Gtk-CRITICAL **: gtk_box_pack: assertion `child->parent == NULL' failed*/
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog1), ok, GTK_RESPONSE_OK);
	GTK_WIDGET_SET_FLAGS (ok, GTK_CAN_DEFAULT);

	GtkWidget *cancel = (GtkWidget *)glade_xml_get_widget (gui, "cancel_button");
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog1), cancel, GTK_RESPONSE_CANCEL);
	GTK_WIDGET_SET_FLAGS (cancel, GTK_CAN_DEFAULT);

	gtk_widget_add_accelerator (ok, "activate", accel_group,
                              GDK_Return, (GdkModifierType) 0,
                              GTK_ACCEL_VISIBLE);
	gtk_widget_add_accelerator (ok, "activate", accel_group,
                              GDK_KP_Enter, (GdkModifierType) 0,
                              GTK_ACCEL_VISIBLE);
	gtk_window_add_accel_group (GTK_WINDOW (dialog1), accel_group);

  	feed->fetch_html = fhtml;
        feed->dialog = dialog1;
        feed->child = child;
	feed->gui = gui;
	if (flabel)
		g_free(flabel);
  	return feed;
}

void
actions_dialog_add(add_feed *feed, gchar *url)
{
        GtkWidget *entry1 = (GtkWidget *)glade_xml_get_widget (feed->gui, "url_entry");
	GtkWidget *checkbutton1 = (GtkWidget *)glade_xml_get_widget (feed->gui, "html_check");
	GtkWidget *checkbutton2 = (GtkWidget *)glade_xml_get_widget (feed->gui, "enabled_check");
	GtkWidget *checkbutton3 = (GtkWidget *)glade_xml_get_widget (feed->gui, "validate_check");
	GtkWidget *checkbutton4 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_unread");
	GtkWidget *radiobutton1 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_rb1");
	GtkWidget *radiobutton2 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_rb2");
	GtkWidget *radiobutton3 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_rb3");
	GtkWidget *radiobutton7 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_rb4");
	GtkWidget *radiobutton4 = (GtkWidget *)glade_xml_get_widget (feed->gui, "ttl_global");
	GtkWidget *radiobutton5 = (GtkWidget *)glade_xml_get_widget (feed->gui, "ttl");
	GtkWidget *radiobutton6 = (GtkWidget *)glade_xml_get_widget (feed->gui, "ttl_disabled");
	GtkWidget *spinbutton1 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_sb1");
	GtkWidget *spinbutton2 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_sb2");
	GtkWidget *ttl_value = (GtkWidget *)glade_xml_get_widget (feed->gui, "ttl_value");
  	gboolean fhtml = feed->fetch_html;

  	gint result = gtk_dialog_run(GTK_DIALOG(feed->dialog));
  	switch (result) {
    	case GTK_RESPONSE_OK:
        	feed->feed_url = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry1)));
        	fhtml = gtk_toggle_button_get_active (
        	        GTK_TOGGLE_BUTTON (checkbutton1));
        	fhtml ^= 1;
        	feed->fetch_html = fhtml;
        	feed->enabled = gtk_toggle_button_get_active(
                	GTK_TOGGLE_BUTTON(checkbutton2));
        	feed->validate = gtk_toggle_button_get_active(
                	GTK_TOGGLE_BUTTON(checkbutton3));
        	guint i=0;
        	while (i<4) {
                	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton1)))
                        	break;
                	i++;
                	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton2)))
                        	break;
                	i++;
                	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton3)))
                        	break;
			i++;
                	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton7)))
                        	break;
        	}
        	feed->del_feed = i;
        	feed->del_unread = gtk_toggle_button_get_active(
                	GTK_TOGGLE_BUTTON(checkbutton4));
		gtk_spin_button_update((GtkSpinButton *)spinbutton1);
        	feed->del_messages = gtk_spin_button_get_value((GtkSpinButton *)spinbutton1);
		gtk_spin_button_update((GtkSpinButton *)spinbutton2);
        	feed->del_days = gtk_spin_button_get_value((GtkSpinButton *)spinbutton2);
        	i=1;
        	while (i<3) {
                	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton4)))
                        	break;
                	i++;
                	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton5)))
                        	break;
                	i++;
                	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton6)))
                        	break;
        	}
        	feed->update=i;
        	feed->ttl = gtk_spin_button_get_value((GtkSpinButton *)ttl_value);
        	feed->add = 1;
        	// there's no reason to feetch feed if url isn't changed
        	if (url && !strncmp(url, feed->feed_url, strlen(url)))
                	feed->changed = 0;
        	else
                	feed->changed = 1;
        	break;
    	default:
        	feed->add = 0;
        	gtk_widget_destroy (feed->dialog);
        	break;
  	}
}


add_feed *
create_dialog_add(gchar *url, gchar *feed_text)
{
	add_feed *feed = NULL;
	feed = build_dialog_add(url, feed_text);
	actions_dialog_add(feed, url);
	return feed;
}

gboolean
store_redraw(GtkTreeView *data)
{
	if (!store_redrawing) {
		store_redrawing = 1;
        	GtkTreeModel *model = gtk_tree_view_get_model (data);
		gtk_list_store_clear(GTK_LIST_STORE(model));
        	g_hash_table_foreach(rf->hrname, construct_list, model);
		store_redrawing = 0;
	}
	return FALSE;
}

////////////////////
//feeds processing//
////////////////////

static void
msg_feeds_response(GtkWidget *selector, guint response, gpointer user_data)
{
        while (gtk_events_pending ())
                gtk_main_iteration ();
        if (response == GTK_RESPONSE_CANCEL)
                rf->cancel = 1;
	gtk_widget_destroy(selector);
}

static void
feeds_dialog_add(GtkDialog *d, gpointer data)
{
        gchar *text;
        add_feed *feed = create_dialog_add(NULL, NULL);
	if (feed->dialog)
                gtk_widget_destroy(feed->dialog);
        GtkWidget *msg_feeds = e_error_new(NULL,
					"org-gnome-evolution-rss:rssmsg",
					"",
					NULL);
	GtkWidget *progress = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(((GtkDialog *)msg_feeds)->vbox),
				 	progress,
					FALSE,
					FALSE,
					0);
        gtk_progress_bar_set_fraction((GtkProgressBar *)progress, 0);
	/* xgettext:no-c-format */
        gtk_progress_bar_set_text((GtkProgressBar *)progress, _("0% done"));
	feed->progress=progress;
        gtk_window_set_keep_above(GTK_WINDOW(msg_feeds), TRUE);
        g_signal_connect(msg_feeds, "response", G_CALLBACK(msg_feeds_response), NULL);
	gtk_widget_show_all(msg_feeds);
        while (gtk_events_pending ())
                gtk_main_iteration ();
        if (feed->feed_url && strlen(feed->feed_url)) {
                text = feed->feed_url;
                feed->feed_url = sanitize_url(feed->feed_url);
                g_free(text);
                if (g_hash_table_find(rf->hr,
                                        check_if_match,
                                        feed->feed_url)) {
                           rss_error(NULL, NULL, _("Error adding feed."),
                                           _("Feed already exists!"));
                           goto out;
                }
                setup_feed(feed);
		store_redraw((GtkTreeView *)data);
                save_gconf_feed();
        }
out:    gtk_widget_destroy(msg_feeds);
        g_free(feed);
}

static void
destroy_delete(GtkWidget *selector, gpointer user_data)
{
        gtk_widget_destroy(user_data);
        rf->import = 0;
}

//this function resembles emfu_delete_rec in mail/em-folder-utils.c
//which is not exported ? 
static void
rss_delete_rec (CamelStore *store, CamelFolderInfo *fi, CamelException *ex)
{
        while (fi) {
                CamelFolder *folder;

                d(printf ("deleting folder '%s'\n", fi->full_name));

                if (!(folder = camel_store_get_folder (store, fi->full_name, 0, ex)))
                        return;

                        GPtrArray *uids = camel_folder_get_uids (folder);
                        int i;

                        camel_folder_freeze (folder);
                        for (i = 0; i < uids->len; i++)
                                camel_folder_delete_message (folder, uids->pdata[i]);

                        camel_folder_free_uids (folder, uids);

                        camel_folder_sync (folder, TRUE, NULL);
                        camel_folder_thaw (folder);

                camel_store_delete_folder (store, fi->full_name, ex);
                if (camel_exception_is_set (ex))
                        return;

                fi = fi->next;
        }
}

void
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

void
destroy_feed_hash_content(hrfeed *s)
{
	g_free(s->hrname);
	g_free(s->hrname_r);
	g_free(s->hrt);
	g_free(s->hr);
	g_free(s);
}

hrfeed*
save_feed_hash(gpointer name)
{
        hrfeed *saved_feed = g_new0(hrfeed, 1);
	saved_feed->hrname = g_strdup(g_hash_table_lookup(rf->hrname, name));
	saved_feed->hrname_r = g_strdup(g_hash_table_lookup(rf->hrname_r, lookup_key(name)));
	saved_feed->hre = GPOINTER_TO_INT(g_hash_table_lookup(rf->hre, lookup_key(name)));
	saved_feed->hrt = g_strdup(g_hash_table_lookup(rf->hrt, lookup_key(name)));
	saved_feed->hrh = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrh, lookup_key(name)));
	saved_feed->hr = g_strdup(g_hash_table_lookup(rf->hr, lookup_key(name)));
	saved_feed->hrdel_feed = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_feed, lookup_key(name)));
	saved_feed->hrdel_days = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_days, lookup_key(name)));
	saved_feed->hrdel_messages = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_messages, lookup_key(name)));
	saved_feed->hrdel_unread = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrdel_unread, lookup_key(name)));
	saved_feed->hrupdate = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrupdate, lookup_key(name)));
	saved_feed->hrttl = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrttl, lookup_key(name)));
	saved_feed->hrttl_multiply = GPOINTER_TO_INT(g_hash_table_lookup(rf->hrttl_multiply, lookup_key(name)));
	return saved_feed;
}

// restores a feed structure removed from hash
// name - key to restore
// s - feed structure to restore
// upon return s structure is destroyed
void
restore_feed_hash(gpointer name, hrfeed *s)
{
	g_hash_table_insert(rf->hrname, g_strdup(name), s->hrname);
	g_hash_table_insert(rf->hrname_r, g_strdup(lookup_key(name)), s->hrname_r);
	g_hash_table_insert(rf->hre, g_strdup(lookup_key(name)), GINT_TO_POINTER(s->hre));
	g_hash_table_insert(rf->hrh, g_strdup(lookup_key(name)), GINT_TO_POINTER(s->hrh));
	g_hash_table_insert(rf->hrt, g_strdup(lookup_key(name)), GINT_TO_POINTER(s->hrt));
	g_hash_table_insert(rf->hr, g_strdup(lookup_key(name)), s->hr);
	g_hash_table_insert(rf->hrdel_feed, g_strdup(lookup_key(name)), GINT_TO_POINTER(s->hrdel_feed));
	g_hash_table_insert(rf->hrdel_days, g_strdup(lookup_key(name)), GINT_TO_POINTER(s->hrdel_days));
	g_hash_table_insert(rf->hrdel_messages, g_strdup(lookup_key(name)), GINT_TO_POINTER(s->hrdel_messages));
	g_hash_table_insert(rf->hrdel_unread, g_strdup(lookup_key(name)), GINT_TO_POINTER(s->hrdel_unread));
	g_hash_table_insert(rf->hrupdate, g_strdup(lookup_key(name)), GINT_TO_POINTER(s->hrupdate));
	g_hash_table_insert(rf->hrttl, g_strdup(lookup_key(name)), GINT_TO_POINTER(s->hrttl));
	g_hash_table_insert(rf->hrttl_multiply, g_strdup(lookup_key(name)), GINT_TO_POINTER(s->hrttl_multiply));
	g_free(s);
}

void
remove_feed_hash(gpointer name)
{
	//we need to make sure we won't fetch_feed iterate over those hashes
	rf->pending = TRUE;
	taskbar_op_finish(name);
        g_hash_table_remove(rf->hre, lookup_key(name));
        g_hash_table_remove(rf->hrt, lookup_key(name));
        g_hash_table_remove(rf->hrh, lookup_key(name));
        g_hash_table_remove(rf->hr, lookup_key(name));
	g_hash_table_remove(rf->hrdel_feed, lookup_key(name));
	g_hash_table_remove(rf->hrdel_days, lookup_key(name));
	g_hash_table_remove(rf->hrdel_messages, lookup_key(name));
	g_hash_table_remove(rf->hrdel_unread, lookup_key(name));
	g_hash_table_remove(rf->hrupdate, lookup_key(name));
	g_hash_table_remove(rf->hrttl, lookup_key(name));
	g_hash_table_remove(rf->hrttl_multiply, lookup_key(name));
        g_hash_table_remove(rf->hrname_r, lookup_key(name));
        g_hash_table_remove(rf->hrname, name);
	rf->pending = FALSE;
}

void
delete_feed_folder_alloc(gchar *old_name)
{
        FILE *f;
	gchar *feed_dir = rss_component_peek_base_directory(mail_component_peek());
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

static void
delete_response(GtkWidget *selector, guint response, gpointer user_data)
{
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        gchar *name;
        if (response == GTK_RESPONSE_OK) {
                selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(user_data));
                if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
                        gtk_tree_model_get (model, &iter, 4, &name, -1);
			rss_delete_feed(name,
				gconf_client_get_bool(rss_gconf, GCONF_KEY_REMOVE_FOLDER, NULL));
                        g_free(name);
                }
		store_redraw(GTK_TREE_VIEW(rf->treeview));
                save_gconf_feed();
        }
        gtk_widget_destroy(selector);
        rf->import = 0;
}

void
feeds_dialog_disable(GtkDialog *d, gpointer data)
{
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        gchar *name;

        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(rf->treeview));
        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
                gtk_tree_model_get (model, &iter, 3, &name, -1);
                gpointer key = lookup_key(name);
                g_free(name);
                g_hash_table_replace(rf->hre, g_strdup(key),
                        GINT_TO_POINTER(!g_hash_table_lookup(rf->hre, key)));
                gtk_button_set_label(data,
                        g_hash_table_lookup(rf->hre, key) ? _("Disable") : _("Enable"));
        }
        //update list instead of rebuilding
        store_redraw(GTK_TREE_VIEW(rf->treeview));
        save_gconf_feed();
}

GtkWidget*
remove_feed_dialog(gchar *msg)
{
  GtkWidget *dialog1;
  GtkWidget *dialog_vbox1;
  GtkWidget *vbox1;
  GtkWidget *checkbutton1;
  GtkWidget *dialog_action_area1;

  dialog1 = e_error_new(NULL, "org-gnome-evolution-rss:ask-delete-feed", msg, NULL);
  gtk_window_set_keep_above(GTK_WINDOW(dialog1), TRUE);

  dialog_vbox1 = GTK_DIALOG (dialog1)->vbox;
  gtk_widget_show (dialog_vbox1);

  vbox1 = gtk_vbox_new (FALSE, 10);
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 10);

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

  return dialog1;
}

void
feeds_dialog_delete(GtkDialog *d, gpointer data)
{
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        gchar *name;

        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data));
        if (gtk_tree_selection_get_selected(selection, &model, &iter)
                && !rf->import) {
                rf->import = 1;
                gtk_tree_model_get (model, &iter, 3, &name, -1);
                GtkWidget *rfd = remove_feed_dialog(name);
                gtk_widget_show(rfd);
                g_signal_connect(rfd, "response", G_CALLBACK(delete_response), data);
                g_signal_connect(rfd, "destroy", G_CALLBACK(destroy_delete), rfd);
                g_free(name);
        }
}

void
process_dialog_edit(add_feed *feed, gchar *url, gchar *feed_name)
{
	gchar *text = NULL;
	gpointer key = lookup_key(feed_name);

	GtkWidget *msg_feeds = e_error_new(NULL, "org-gnome-evolution-rss:rssmsg", "", NULL);
	GtkWidget *progress = gtk_progress_bar_new();
       	gtk_box_pack_start(GTK_BOX(((GtkDialog *)msg_feeds)->vbox), progress, FALSE, FALSE, 0);
       	gtk_progress_bar_set_fraction((GtkProgressBar *)progress, 0);
	/* xgettext:no-c-format */
       	gtk_progress_bar_set_text((GtkProgressBar *)progress, _("0% done"));
	feed->progress=progress;
       	gtk_window_set_keep_above(GTK_WINDOW(msg_feeds), TRUE);
	g_signal_connect(msg_feeds, "response", G_CALLBACK(msg_feeds_response), NULL);
	gtk_widget_show_all(msg_feeds);
       	while (gtk_events_pending ())
       		gtk_main_iteration ();
	if (!feed->add)
                 goto out;
        text = feed->feed_url;
        feed->feed_url = sanitize_url(feed->feed_url);
        g_free(text);
        if (feed->feed_url) {
		if (strcmp(url, feed->feed_url)) {
			//prevent adding of an existing feed (url)
			//which might screw things
                        if (g_hash_table_find(rf->hr,
                                                check_if_match,
						feed->feed_url)) {
				rss_error(NULL, NULL, _("Error adding feed."),
                                                        _("Feed already exists!"));
                                                goto out;
			}
			hrfeed *saved_feed = save_feed_hash(feed_name);
                       	remove_feed_hash(feed_name);
                        gpointer md5 = gen_md5(feed->feed_url);
			if (!setup_feed(feed)) {
				//editing might loose a corectly setup feed
				//so re-add previous deleted feed
				restore_feed_hash(key, saved_feed);
			} else
				destroy_feed_hash_content(saved_feed);
			g_free(md5);
		} else {
			key = gen_md5(url);
			g_hash_table_replace(rf->hrh,
                                                        g_strdup(key),
                                                        GINT_TO_POINTER(feed->fetch_html));
			if (feed->update == 2) {
				g_hash_table_replace(rf->hrttl,
						g_strdup(key),
						GINT_TO_POINTER(feed->ttl));
                              	g_hash_table_replace(rf->hrttl_multiply,
						g_strdup(key),
						GINT_TO_POINTER(feed->ttl_multiply));
				custom_feed_timeout();
			}
			if (feed->update == 3)
				g_hash_table_replace(rf->hre,
						g_strdup(key),
						0);
			else {
                              	g_hash_table_replace(rf->hre,
						g_strdup(key),
						GINT_TO_POINTER(feed->enabled));
			} 

			if (feed->renamed) {
				gchar *a = g_build_path("/", lookup_main_folder(), lookup_feed_folder(feed_name), NULL);
				gchar *dir = g_path_get_dirname(a);
				gchar *b = g_build_path("/", dir, feed->feed_name, NULL);
				CamelException ex;
				camel_exception_init (&ex);
				CamelStore *store = mail_component_peek_local_store(NULL);
                                camel_store_rename_folder (store, a, b, &ex);
                                if (camel_exception_is_set (&ex)) {
                                        e_error_run(NULL,
                                                    "mail:no-rename-folder", a, b, ex.desc, NULL);
                                        camel_exception_clear (&ex);
                                }
				g_free(dir);
				g_free(b);
				g_free(a);
			}

			g_hash_table_replace(rf->hrdel_feed,
					g_strdup(key),
					GINT_TO_POINTER(feed->del_feed));
			g_hash_table_replace(rf->hrdel_days,
					g_strdup(key),
					GINT_TO_POINTER(feed->del_days));
			g_hash_table_replace(rf->hrdel_messages,
					g_strdup(key),
					GINT_TO_POINTER(feed->del_messages));
					g_hash_table_replace(rf->hrupdate,
					g_strdup(key),
					GINT_TO_POINTER(feed->update));
			g_hash_table_replace(rf->hrdel_unread,
					g_strdup(key),
					GINT_TO_POINTER(feed->del_unread));
			g_free(key);
		}
	save_gconf_feed();
	}
out:	gtk_widget_destroy(msg_feeds);
	g_free(feed);
}

static void
feeds_dialog_edit(GtkDialog *d, gpointer data)
{
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;
        gchar *name, *feed_name;
	gpointer key;
	add_feed *feed = NULL;

        /* This will only work in single or browse selection mode! */
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data));
        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
                gtk_tree_model_get (model, &iter, 3, &feed_name, -1);
                key = lookup_key(feed_name);
                name = g_hash_table_lookup(rf->hr, key);
                if (name) {
                        feed = create_dialog_add(name, feed_name);
                    	if (feed->dialog)
                                gtk_widget_destroy(feed->dialog);
			process_dialog_edit(feed, name, feed_name);
		}
        	if (feed->feed_url)
			store_redraw(GTK_TREE_VIEW(rf->treeview));
	}
}

void
import_dialog_response(GtkWidget *selector, guint response, gpointer user_data)
{
        while (gtk_events_pending ())
                gtk_main_iteration ();
        if (response == GTK_RESPONSE_CANCEL)
                rf->cancel = 1;
}

gboolean
import_one_feed(gchar *url, gchar *title, gchar *prefix)
{
        add_feed *feed = g_new0(add_feed, 1);
        feed->changed=0;
        feed->add=1;
        feed->fetch_html = feed_html;
        feed->validate = feed_validate;
	feed->enabled = feed_enabled;
	feed->feed_url = g_strdup(url);
	feed->feed_name = decode_html_entities(title);
	feed->prefix = prefix;
	/* we'll get rid of this as soon as we fetch unblocking */
        if (g_hash_table_find(rf->hr,
                                     check_if_match,
                                     feed->feed_url)) {
               rss_error(NULL, feed->feed_name, _("Error adding feed."),
                                _("Feed already exists!"));
               return FALSE;
        }
        guint res = setup_feed(feed);
        d(g_print("feed imported:%d\n", res));
        g_free(feed->feed_url);
        g_free(feed->feed_name);
	g_free(feed);
	return res;
}

/*
 * type 0/1 - opml/foaf
 */

xmlNode*
iterate_import_file(xmlNode *src, gchar **url, xmlChar **title, guint type)
{
	*url = NULL;
	*title = NULL;

	if (type == 0) {
        	src = html_find(src, "outline");
        	*url = (gchar *)xmlGetProp(src, (xmlChar *)"xmlUrl");
		*title = xmlGetProp(src, (xmlChar *)"title");
		if (!(*title = xmlGetProp(src, (xmlChar *)"title")))
			*title = xmlGetProp(src, (xmlChar *)"text");
	} else if (type == 1) {
		xmlNode *my;
		src = html_find(src, "member");
		my = layer_find_pos(src, "member", "Agent");
		*title = xmlCharStrdup(layer_find(my, "name", NULL));
		my =  html_find(my, "Document");
		*url =  (gchar *)xmlGetProp(my, (xmlChar *)"about");
		if (!*url) {
			my =  html_find(my, "channel");
			*url =  (gchar *)xmlGetProp(my, (xmlChar *)"about");
		}
	}
	return src;
	
}

void
import_opml(gchar *file)
{
	gchar *url = NULL;
	xmlChar *name = NULL;
        guint total = 0;
        guint current = 0;
	guint type = 0; //file type
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
	if ((src=src->children)) {
		if (!g_ascii_strcasecmp((char *)src->name, "rdf")) {
			while (src) {
				src=src->children;
				src = src->next;
				src = src->children;
				d(g_print("group name:%s\n", layer_find(src, "name", NULL)));
				src = src->next;
				while ((src = iterate_import_file(src, &url, &name, 1))) {
                			if (url) {
                        			total++;
                        			xmlFree(url);
                			}
					if (name) xmlFree(name);
				}
			d(g_print("total:%d\n", total));
			type = 1;
			}
		}
		else if (!g_ascii_strcasecmp((char *)src->name, "opml")) {
			
			while ((src = iterate_import_file(src, &url, &name, 0))) {
                		if (url && strlen(url)) {
                        		total++;
                        		xmlFree(url);
                		}
				if (name) xmlFree(name);
        		}
			type = 0;
			d(g_print("total:%d\n", total));
		}
	}	
        src = doc;
//	//force out for now
//	goto out;
        //we'll be safer this way
        rf->import = 1;
	name = NULL;
        while (gtk_events_pending ())
                gtk_main_iteration ();
	if (type == 1) {
		src=src->children;
		d(g_print("my cont:%s\n", src->content));
		src=src->children;
		src = src->next;
		d(g_print("found %s\n", src->name));
		src = src->children;
		d(g_print("group name:%s\n", layer_find(src, "name", NULL)));
		src = src->next;
	}

	if (type == 0) {
	gint size = 0;
	gchar *base = NULL, *root = NULL, *last = NULL;
	gchar *rssprefix = NULL, *rssurl = NULL, *rsstitle = NULL;
	while (src) {
		if (rf->cancel) {
			if (src) xmlFree(src);
			rf->cancel = 0;
			goto out;
		}
                if (src->children)
                        src = src->children;
                else {
                        while (src && !src->next) {
                                src = src->parent;
				g_print("<-");
				last =  g_path_get_basename(root);
				if (last && strcmp(last, ".")) {
					g_print("retract:%s\n", last);
					size = strstr(root, last)-root-1;
					gchar *tmp = root;
					if (size > 0)
						root = g_strndup(root, size);
					else
						root = NULL;
					g_free(last);
					if (tmp) g_free(tmp);
				}
			}
                        if (!src) break;
                        src = src->next;
                }
                if (src->name) {
			gchar *prop = (gchar *)xmlGetProp(src, (xmlChar *)"type");
			if (prop) {
				if (!strcmp(prop, "folder")) {
					base = (gchar *)xmlGetProp(src, (xmlChar *)"text");
					if (NULL != src->last) {
						gchar *tmp = root;
						if (!root)
							root = g_build_path("/", base, NULL);
						else
							root = g_build_path("/", root, base, NULL);
						if (base) xmlFree(base);
						if (tmp) g_free(tmp);
					}
				// we're insterested in rss/pie only
				// we might just handle all variations of type= property
				} else if (strcmp(prop, "link")) {
					rssprefix = root;
					rssurl = (gchar *)xmlGetProp(src, (xmlChar *)"xmlUrl");
					rsstitle = (gchar *)xmlGetProp(src, (xmlChar *)"title");
					gtk_label_set_text(GTK_LABEL(import_label), (gchar *)rsstitle);
#if GTK_VERSION >= 2006000
					gtk_label_set_ellipsize (GTK_LABEL (import_label), PANGO_ELLIPSIZE_START);
#endif
					gtk_label_set_justify(GTK_LABEL(import_label), GTK_JUSTIFY_CENTER);
					import_one_feed(rssurl, rsstitle, rssprefix);
					if (rssurl) xmlFree(rssurl);
					if (rsstitle) xmlFree(rsstitle);
                        		while (gtk_events_pending ())
                                		gtk_main_iteration ();
					current++;
					float fr = ((current*100)/total);
					gtk_progress_bar_set_fraction((GtkProgressBar *)import_progress, fr/100);
					what = g_strdup_printf(_("%2.0f%% done"), fr);
					gtk_progress_bar_set_text((GtkProgressBar *)import_progress, what);
					g_free(what);
					while (gtk_events_pending ())
						gtk_main_iteration ();
					store_redraw(GTK_TREE_VIEW(rf->treeview));
					save_gconf_feed();
				}
			xmlFree(prop);
			}
                }
        }
		goto out;
	}

	while ((src = iterate_import_file(src, &url, &name, type))) {
                if (url && strlen(url)) {
			d(g_print("url:%s\n", url));
                        if (rf->cancel) {
                                if (src) xmlFree(src);
                                rf->cancel = 0;
                                goto out;
                        }
                        gtk_label_set_text(GTK_LABEL(import_label), (gchar *)name);
#if GTK_VERSION >= 2006000
                        gtk_label_set_ellipsize (GTK_LABEL (import_label), PANGO_ELLIPSIZE_START);
#endif
                        gtk_label_set_justify(GTK_LABEL(import_label), GTK_JUSTIFY_CENTER);
			import_one_feed(url, (gchar *)name, NULL);
			if (name) xmlFree(name);
			if (url) xmlFree(url);

                        while (gtk_events_pending ())
                                gtk_main_iteration ();
                        current++;
                        float fr = ((current*100)/total);
                        gtk_progress_bar_set_fraction((GtkProgressBar *)import_progress, fr/100);
                        what = g_strdup_printf(_("%2.0f%% done"), fr);
                        gtk_progress_bar_set_text((GtkProgressBar *)import_progress, what);
                        g_free(what);
                        while (gtk_events_pending ())
                                gtk_main_iteration ();
			store_redraw(GTK_TREE_VIEW(rf->treeview));
                        save_gconf_feed();
		}
        }
        while (gtk_events_pending ())
                gtk_main_iteration ();
out:    rf->import = 0;
        xmlFree(doc);
        gtk_widget_destroy(import_dialog);
}

static void
select_file_response(GtkWidget *selector, guint response, gpointer user_data)
{
        if (response == GTK_RESPONSE_OK) {
                char *name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (selector));
                if (name) {
                        gtk_widget_hide(selector);
                        import_opml(name);
                        g_free(name);
                }
        } else
                gtk_widget_destroy(selector);
}

static void
import_toggle_cb_html (GtkWidget *widget, gpointer data)
{
        feed_html  = 1-gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void
import_toggle_cb_valid (GtkWidget *widget, gpointer data)
{
        feed_validate  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void
import_toggle_cb_ena (GtkWidget *widget, gpointer data)
{
        feed_enabled  = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
}

static void
decorate_import_fs (gpointer data)
{
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
        feed_html = 0;
        feed_validate = feed_enabled = 1;

        g_signal_connect(checkbutton1,
                        "toggled",
                        G_CALLBACK(import_toggle_cb_html),
                        NULL);
        g_signal_connect(checkbutton2,
                        "toggled",
                        G_CALLBACK(import_toggle_cb_ena),
                        NULL);
        g_signal_connect(checkbutton3,
                        "toggled",
                        G_CALLBACK(import_toggle_cb_valid),
                        NULL);
        g_signal_connect(data, "response", G_CALLBACK(select_file_response), NULL);
        g_signal_connect(data, "destroy", G_CALLBACK(gtk_widget_destroy), data);
}

GtkWidget*
create_import_dialog (void)
{
  GtkWidget *import_file_select;
  GtkWidget *dialog_vbox5;
  GtkWidget *dialog_action_area5;
  GtkWidget *button1;
  GtkWidget *button2;

  import_file_select = gtk_file_chooser_dialog_new (_("Select import file"),
				 NULL, GTK_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);
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

GtkWidget*
create_export_dialog (void)
{
  GtkWidget *export_file_select;
  GtkWidget *vbox26;
  GtkWidget *hbuttonbox1;
  GtkWidget *button3;
  GtkWidget *button4;

  export_file_select = gtk_file_chooser_dialog_new (_("Select file to export"), 
				NULL, GTK_FILE_CHOOSER_ACTION_SAVE, NULL, NULL);
  gtk_window_set_keep_above(GTK_WINDOW(export_file_select), TRUE);
  g_object_set (export_file_select,
                "local-only", FALSE,
                NULL);
  gtk_window_set_modal (GTK_WINDOW (export_file_select), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (export_file_select), TRUE);
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

GtkWidget*
create_import_cookies_dialog (void)
{
  GtkWidget *import_file_select;
  GtkWidget *vbox26;
  GtkWidget *hbuttonbox1;
  GtkWidget *button3;
  GtkWidget *button4;

  import_file_select = gtk_file_chooser_dialog_new (_("Select file to import"),
                                NULL, GTK_FILE_CHOOSER_ACTION_SAVE, NULL, NULL);
  gtk_window_set_keep_above(GTK_WINDOW(import_file_select), TRUE);
  g_object_set (import_file_select,
                "local-only", FALSE,
                NULL);
  gtk_window_set_modal (GTK_WINDOW (import_file_select), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (import_file_select), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (import_file_select), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (import_file_select), GDK_WINDOW_TYPE_HINT_DIALOG);

  vbox26 = GTK_DIALOG (import_file_select)->vbox;
  gtk_widget_show (vbox26);

  hbuttonbox1 = GTK_DIALOG (import_file_select)->action_area;
  gtk_widget_show (hbuttonbox1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);

  button3 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (button3);
  gtk_dialog_add_action_widget (GTK_DIALOG (import_file_select), button3, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (button3, GTK_CAN_DEFAULT);

  button4 = gtk_button_new_from_stock ("gtk-save");
  gtk_widget_show (button4);
  gtk_dialog_add_action_widget (GTK_DIALOG (import_file_select), button4, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (button4, GTK_CAN_DEFAULT);

  gtk_widget_grab_default (button4);
  return import_file_select;
}
static void
import_cookies_cb (GtkWidget *widget, gpointer data)
{
	GtkWidget *import = create_import_cookies_dialog();
	decorate_import_cookies_fs(import);
	gtk_widget_show(import);
}

static void
import_cb (GtkWidget *widget, gpointer data)
{
        if (!rf->import) {
                GtkWidget *import = create_import_dialog();
                decorate_import_fs(import);
                gtk_widget_show(import);
        }
        return;
}

static void
get_folder_info (CamelStore *store, CamelFolderInfo *info, CamelException *ex)
{
while (info) {
                CamelFolder *fold;
		gchar **path, *fpath;
		gint i=0;

                if (info->child) {
			get_folder_info(store, info->child, ex);
                        if (camel_exception_is_set (ex))
                                return;
                }

                if (!(fold = camel_store_get_folder (store, info->full_name, 0, ex)))
                        return;

//		g_print("fold:%s\n", fold->full_name);
		fpath = extract_main_folder(fold->full_name);
		g_print("fpath:%s\n", fpath);
	
		path = g_strsplit(fpath, "/", 0);
		if (path) {
			do {
				g_print("path:%s\n", path[i]);
			} while (NULL != path[i++]);
		}

                info = info->next;
        }


}

static void
construct_opml_line(gpointer key, gpointer value, gpointer user_data)
{
        gchar *url = g_hash_table_lookup(rf->hr, value);
        gchar *type = g_hash_table_lookup(rf->hrt, value);
        gchar *url_esc = g_markup_escape_text(url, strlen(url));
        gchar *key_esc = g_markup_escape_text(key, strlen(key));

#if 0
CamelFolderInfo *info;
	CamelStore *store = mail_component_peek_local_store(NULL);
        CamelException ex;
        guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE | CAMEL_STORE_FOLDER_INFO_FAST;


        camel_exception_init (&ex);

        info = camel_store_get_folder_info (store, "News and Blogs", flags, &ex);

        if (camel_exception_is_set (&ex))
                goto out;
	get_folder_info(store, info, &ex);

out:
        camel_store_free_folder_info(store, info);
#endif

        //gchar *tmp = g_strdup_printf("<outline text=\"%s\" title=\"%s\" type=\"%s\" xmlUrl=\"%s\" htmlUrl=\"%s\"/>\n",
        gchar *tmp = g_strdup_printf("<outline text=\"%s\" title=\"%s\" type=\"rss\" xmlUrl=\"%s\" htmlUrl=\"%s\"/>\n",
                key_esc, key_esc, url_esc, url_esc);
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

#if 0
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
#endif

over:   f = fopen(file, "w+");
        if (f) {
                fwrite(opml, strlen(opml), 1, f);
                fclose(f);
        } else {
                e_error_run(NULL,
                        "org-gnome-evolution-rss:feederr",
                        _("Error exporting feeds!"),
                        g_strerror(errno),
                        NULL);
        }
out:    g_free(opml);

}

SoupCookieJar *
import_cookies(gchar *file)
{
	SoupCookieJar *jar = NULL;
	gchar header[16];
	memset(header, 0, 16);
	d(g_print("import cookies from %s\n", file));
	FILE *f = fopen(file, "r");
	if (f) {
		fgets(header, 16, f);
		fclose(f);
		if (!g_ascii_strncasecmp(header, SQLITE_MAGIC, sizeof(SQLITE_MAGIC))) {
#if LIBSOUP_VERSION > 2026002 && defined(HAVE_LIBSOUP_GNOME)
			jar = soup_cookie_jar_sqlite_new(file, TRUE);
#endif
		} else
			jar = soup_cookie_jar_text_new(file, TRUE);
	}
	return jar;
}

void
inject_cookie(SoupCookie *cookie, GtkProgressBar *progress)
{
	gchar *text;
	ccurrent++;
	if (!rf->cancel) {
		float fr = ((ccurrent*100)/ctotal);
		gtk_progress_bar_set_fraction(progress, fr/100);
		text = g_strdup_printf(_("%2.0f%% done"), fr);
		gtk_progress_bar_set_text(progress, text);
		g_free(text);
		soup_cookie_jar_add_cookie(rss_soup_jar, cookie);
		while (gtk_events_pending ())
			gtk_main_iteration ();
	}
}

void
process_cookies(SoupCookieJar *jar)
{
	ccurrent = 0;
	ctotal = 0;
	GSList *list = soup_cookie_jar_all_cookies(jar);
        gchar *msg = g_strdup(_("Importing cookies..."));
        GtkWidget *import_dialog = e_error_new(NULL, "shell:importing", msg, NULL);
        gtk_window_set_keep_above(GTK_WINDOW(import_dialog), TRUE);
        g_signal_connect(import_dialog, "response", G_CALLBACK(import_dialog_response), NULL);
        GtkWidget *import_label = gtk_label_new(_("Please wait"));
        GtkWidget *import_progress = gtk_progress_bar_new();
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
	ctotal = g_slist_length(list);
	g_slist_foreach(list, (GFunc)inject_cookie, import_progress);
	//reset cancel signal
	rf->cancel = 0;
	gtk_widget_destroy(import_dialog);
	//copy gecko data over (gecko will open database locked exclusively)
	sync_gecko_cookies();
}

static void
select_export_response(GtkWidget *selector, guint response, gpointer user_data)
{
        if (response == GTK_RESPONSE_OK) {
                char *name;

                name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (selector));
                if (name) {
                        gtk_widget_destroy(selector);
                        export_opml(name);
                        g_free(name);
                }
        } else
                gtk_widget_destroy(selector);

}

static void
select_import_cookies_response(GtkWidget *selector, guint response, gpointer user_data)
{
	SoupCookieJar *jar;

        if (response == GTK_RESPONSE_OK) {
                char *name;

                name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (selector));
                if (name) {
                        gtk_widget_destroy(selector);
                        if ((jar = import_cookies(name)))
				process_cookies(jar);
                        g_free(name);
                }
        } else
                gtk_widget_destroy(selector);

}

static void
decorate_export_fs (gpointer data)
{
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

void
decorate_import_cookies_fs (gpointer data)
{
        gtk_dialog_set_default_response (GTK_DIALOG (data), GTK_RESPONSE_OK);
        gtk_file_chooser_set_local_only (data, FALSE);

        GtkFileFilter *file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("All Files"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));

        file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*.txt");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("Mozilla/Netscape Format"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));

        file_filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (GTK_FILE_FILTER(file_filter), "*.sqlite");
        gtk_file_filter_set_name (GTK_FILE_FILTER(file_filter), _("Firefox new Format"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));

        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (data),
                                        GTK_FILE_FILTER(file_filter));


        GtkFileFilter *filter = gtk_file_filter_new ();
        gtk_file_filter_add_pattern (filter, "*.txt");
        gtk_file_filter_add_pattern (filter, "*.sqilte");
        gtk_file_chooser_set_filter(data, filter);
        g_signal_connect(data, "response", G_CALLBACK(select_import_cookies_response), data);
        g_signal_connect(data, "destroy", G_CALLBACK(gtk_widget_destroy), data);
}

static void
export_cb (GtkWidget *widget, gpointer data)
{
        if (!rf->import) {
                GtkWidget *export = create_export_dialog();
                decorate_export_fs(export);
                gtk_dialog_set_default_response (GTK_DIALOG (export), GTK_RESPONSE_OK);
                if (g_hash_table_size(rf->hrname)<1) {
                        e_error_run(NULL,
                                "org-gnome-evolution-rss:generr",
                                _("No RSS feeds configured!\nUnable to export."),
                                NULL);
                        return;
                }
                gtk_widget_show(export);

//              g_signal_connect(data, "response", G_CALLBACK(select_export_response), data);
//              g_signal_connect(data, "destroy", G_CALLBACK(gtk_widget_destroy), data);
        }
        return;
}


static void
network_timeout_cb (GtkWidget *widget, gpointer data)
{
    gconf_client_set_float (rss_gconf, GCONF_KEY_NETWORK_TIMEOUT,
                gtk_spin_button_get_value((GtkSpinButton*)widget), NULL);
}

static void
destroy_ui_data (gpointer data)
{
        UIData *ui = (UIData *) data;

        if (!ui)
                return;

        g_object_unref (ui->xml);
        g_object_unref (ui->gconf);
        g_free (ui);
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	GtkListStore  *store;
        GtkTreeIter iter;
        GtkWidget *hbox;
	guint i;

        UIData *ui = g_new0 (UIData, 1);

        char *gladefile;

        gladefile = g_build_filename (EVOLUTION_GLADEDIR,
                        "rss-html-rendering.glade",
                        NULL);
        ui->xml = glade_xml_new (gladefile, "settingsbox", GETTEXT_PACKAGE);
        g_free (gladefile);


	ui->combobox = glade_xml_get_widget(ui->xml, "hbox1");
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

        switch (render) {
                case 10:
                        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
                        break;
                case 1:
#ifndef HAVE_WEBKIT
                        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
                        break;
#endif
                case 2:
#ifndef HAVE_GECKO
                        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
                        break;
#endif
                default:
                        g_print("Selected render not supported! Failling back to default.\n");
                        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), render);

        }

        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo),
                                        renderer,
                                        set_sensitive,
                                        NULL, NULL);

#if !defined(HAVE_GECKO) && !defined (HAVE_WEBKIT)
        GtkWidget *label_webkit = glade_xml_get_widget(ui->xml, "label_webkits");
        gtk_label_set_text(GTK_LABEL(label_webkit), _("Note: In order to be able to use Mozilla (Firefox) or Apple Webkit \nas renders you need firefox or webkit devel package \ninstalled and evolution-rss should be recompiled to see those packages."));
        gtk_widget_show(label_webkit);
#endif
        g_signal_connect (combo, "changed", G_CALLBACK (render_engine_changed), NULL);
        gtk_widget_show(combo);
        gtk_box_pack_start(GTK_BOX(ui->combobox), combo, FALSE, FALSE, 0);

	ui->check1 = glade_xml_get_widget(ui->xml, "enable_java");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->check1),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_HTML_JAVA, NULL));
	g_signal_connect(ui->check1, 
		"clicked", 
		G_CALLBACK(start_check_cb), 
		GCONF_KEY_HTML_JAVA);

	ui->check2 = glade_xml_get_widget(ui->xml, "enable_js");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->check2),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_HTML_JS, NULL));
	g_signal_connect(ui->check2, 
		"clicked", 
		G_CALLBACK(start_check_cb), 
		GCONF_KEY_HTML_JS);

	ui->check6 = glade_xml_get_widget(ui->xml, "accept_cookies");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->check6),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_ACCEPT_COOKIES, NULL));

	ui->import = glade_xml_get_widget(ui->xml, "import_cookies");
	//we have to have ui->import looked up
	g_signal_connect(ui->check6, 
		"clicked", 
		G_CALLBACK(accept_cookies_cb), 
		ui->import);

	g_signal_connect(ui->import, "clicked", G_CALLBACK(import_cookies_cb), ui->import);

	ui->nettimeout = glade_xml_get_widget(ui->xml, "nettimeout");
  	gdouble adj = gconf_client_get_float(rss_gconf, GCONF_KEY_NETWORK_TIMEOUT, NULL);
	if (adj < NETWORK_MIN_TIMEOUT) {
		adj = 60;
    		gconf_client_set_float (rss_gconf, GCONF_KEY_NETWORK_TIMEOUT, adj, NULL);
	}
  	if (adj)
		gtk_spin_button_set_value((GtkSpinButton *)ui->nettimeout, adj);
	g_signal_connect(ui->nettimeout, "changed", G_CALLBACK(network_timeout_cb), ui->nettimeout);
	g_signal_connect(ui->nettimeout, "value-changed", G_CALLBACK(network_timeout_cb), ui->nettimeout);

	//feed notification
	
	ui->check3 = glade_xml_get_widget(ui->xml, "status_icon");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->check3),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_STATUS_ICON, NULL));
	g_signal_connect(ui->check3, 
		"clicked", 
		G_CALLBACK(start_check_cb), 
		GCONF_KEY_STATUS_ICON);
	ui->check4 = glade_xml_get_widget(ui->xml, "blink_icon");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->check4),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_BLINK_ICON, NULL));
	g_signal_connect(ui->check4, 
		"clicked", 
		G_CALLBACK(start_check_cb), 
		GCONF_KEY_BLINK_ICON);
	ui->check5 = glade_xml_get_widget(ui->xml, "feed_icon");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->check5),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_FEED_ICON, NULL));
	g_signal_connect(ui->check5, 
		"clicked", 
		G_CALLBACK(start_check_cb), 
		GCONF_KEY_FEED_ICON);
	

        ui->gconf = gconf_client_get_default ();
	hbox = gtk_vbox_new (FALSE, 0);

        gtk_box_pack_start (GTK_BOX (hbox), glade_xml_get_widget (ui->xml, "settingsbox"), FALSE, FALSE, 0);

	g_object_set_data_full (G_OBJECT (hbox), "ui-data", ui, destroy_ui_data);

        return hbox;
}

typedef struct _EConfigTargetRSS EConfigTargetRSS;
struct _EConfigTargetRSS
{
	gchar *label;
} ER;

void rss_folder_factory_abort (EPlugin *epl, EConfigTarget *target)
{
	g_print("abort");
}

void rss_folder_factory_commit (EPlugin *epl, EConfigTarget *target)
{
	const gchar *user = NULL, *pass = NULL;
	add_feed *feed = (add_feed *)g_object_get_data((GObject *)epl, "add-feed");
	gchar *url = (gchar *)g_object_get_data((GObject *)epl, "url");
	gchar *ofolder = (gchar *)g_object_get_data((GObject *)epl, "ofolder");
	

	GtkWidget *entry1 = (GtkWidget *)glade_xml_get_widget (feed->gui, "url_entry");
        GtkWidget *checkbutton1 = (GtkWidget *)glade_xml_get_widget (feed->gui, "html_check");
        GtkWidget *checkbutton2 = (GtkWidget *)glade_xml_get_widget (feed->gui, "enabled_check");
        GtkWidget *checkbutton3 = (GtkWidget *)glade_xml_get_widget (feed->gui, "validate_check");
        GtkWidget *checkbutton4 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_unread");
        GtkWidget *radiobutton1 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_rb1");
        GtkWidget *radiobutton2 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_rb2");
        GtkWidget *radiobutton3 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_rb3");
        GtkWidget *radiobutton7 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_rb4");
        GtkWidget *radiobutton4 = (GtkWidget *)glade_xml_get_widget (feed->gui, "ttl_global");
        GtkWidget *radiobutton5 = (GtkWidget *)glade_xml_get_widget (feed->gui, "ttl");
        GtkWidget *radiobutton6 = (GtkWidget *)glade_xml_get_widget (feed->gui, "ttl_disabled");
        GtkWidget *spinbutton1 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_sb1");
        GtkWidget *spinbutton2 = (GtkWidget *)glade_xml_get_widget (feed->gui, "storage_sb2");
        GtkWidget *ttl_value = (GtkWidget *)glade_xml_get_widget (feed->gui, "ttl_value");
        GtkWidget *feed_name_entry = (GtkWidget *)glade_xml_get_widget (feed->gui, "feed_name");
	gchar *feed_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(feed_name_entry)));

        gboolean fhtml = feed->fetch_html;
	feed->feed_url = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry1)));
        fhtml = gtk_toggle_button_get_active (
                        GTK_TOGGLE_BUTTON (checkbutton1));
	fhtml ^= 1;
        feed->fetch_html = fhtml;
	feed->enabled = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(checkbutton2));
                feed->validate = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(checkbutton3));
                guint i=0;
                while (i<4) {
                        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton1)))
                                break;
                        i++;
                        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton2)))
                                break;
                        i++;
                        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton3)))
                                break;
                        i++;
                        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton7)))
                                break;
                }
                feed->del_feed=i;
                feed->del_unread = gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(checkbutton4));
                gtk_spin_button_update((GtkSpinButton *)spinbutton1);
                feed->del_messages = gtk_spin_button_get_value((GtkSpinButton *)spinbutton1);
                gtk_spin_button_update((GtkSpinButton *)spinbutton2);
                feed->del_days = gtk_spin_button_get_value((GtkSpinButton *)spinbutton2);
                i=1;
                while (i<3) {
                        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton4)))
                                break;
                        i++;
                        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton5)))
                                break;
                        i++;
                        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton6)))
                                break;
                }
                feed->update=i;
                feed->ttl = gtk_spin_button_get_value((GtkSpinButton *)ttl_value);
                feed->add = 1;
		feed->feed_name = feed_name;
                // there's no reason to feetch feed if url isn't changed
		if (url && !strncmp(url, feed->feed_url, strlen(url)))
			feed->changed = 0;
		else
			feed->changed = 1;
		if (feed_name && !g_ascii_strncasecmp(feed_name, ofolder, strlen(feed_name)))
			feed->renamed = 0;
		else
			feed->renamed = 1;
		process_dialog_edit(feed, url, ofolder);
	   
	GtkWidget *authuser = (GtkWidget *)glade_xml_get_widget (feed->gui, "auth_user");
	GtkWidget *authpass = (GtkWidget *)glade_xml_get_widget (feed->gui, "auth_pass");
	GtkWidget *useauth = (GtkWidget *)glade_xml_get_widget (feed->gui, "use_auth");
	
	user = gtk_entry_get_text(GTK_ENTRY(authuser));
	pass = gtk_entry_get_text(GTK_ENTRY(authpass));
	gboolean auth_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (useauth));

	if (user)
		g_hash_table_remove(rf->hruser, url);

        if (pass)
		g_hash_table_remove(rf->hrpass, url);

	if (auth_enabled) {
		g_hash_table_insert(rf->hruser, url, 
			g_strdup(gtk_entry_get_text (GTK_ENTRY (authuser))));
		g_hash_table_insert(rf->hrpass, url, 
			g_strdup(gtk_entry_get_text (GTK_ENTRY (authpass))));
		save_up(url);
	} else
		del_up(url);
}

GtkWidget *
rss_folder_factory (EPlugin *epl, EConfigHookItemFactoryData *data)
{
        EMConfigTargetFolder *target = (EMConfigTargetFolder *)data->config->target;
	gchar *url = NULL, *ofolder = NULL;
	gchar *main_folder = lookup_main_folder();
	gchar *folder = target->folder->full_name;
	add_feed *feed = NULL;

	//filter only rss folders
	if (folder == NULL
          || g_ascii_strncasecmp(folder, main_folder, strlen(main_folder))
          || !g_ascii_strcasecmp(folder, main_folder))
                goto out;

	ofolder = lookup_original_folder(folder);
	gpointer key = lookup_key(ofolder);
	if (!key) {
		g_free(ofolder);
		goto out;
	}

	url = g_hash_table_lookup(rf->hr, key);
	if (url) {
		feed = build_dialog_add(url, ofolder);
		//we do not need ok/cancel buttons here
		GtkWidget *action_area = gtk_dialog_get_action_area(GTK_DIALOG(feed->dialog));
		gtk_widget_hide(action_area);
		gtk_widget_ref(feed->child);
		gtk_container_remove (GTK_CONTAINER (feed->child->parent), feed->child);
		gtk_notebook_remove_page((GtkNotebook *) data->parent, 0);
		gtk_notebook_insert_page((GtkNotebook *) data->parent, (GtkWidget *) feed->child, NULL, 0);
		g_object_set_data_full (G_OBJECT (epl), "add-feed", feed, NULL);
		g_object_set_data_full (G_OBJECT (epl), "url", url, NULL);
		g_object_set_data_full (G_OBJECT (epl), "ofolder", ofolder, NULL);
		return feed->child;
	}

out:	return NULL;
}

/*=============*
 * BONOBO part *
 *=============*/

EvolutionConfigControl*
rss_config_control_new (void)
{
        GtkWidget *control_widget;
        char *gladefile;
	setupfeed *sf;
	
	GtkListStore  *store;
	GtkTreeIter    iter;
	GtkCellRenderer *cell;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;

	d(g_print("rf->%p\n", rf));
	sf = g_new0(setupfeed, 1);

        gladefile = g_build_filename (EVOLUTION_GLADEDIR,
                                      "rss-ui.glade",
                                      NULL);
        sf->gui = glade_xml_new (gladefile, NULL, GETTEXT_PACKAGE);
        g_free (gladefile);

        GtkTreeView *treeview = (GtkTreeView *)glade_xml_get_widget (sf->gui, "feeds-treeview");
	rf->treeview = (GtkWidget *)treeview;
	sf->treeview = (GtkWidget *)treeview;

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

	store = gtk_list_store_new (5, G_TYPE_BOOLEAN, G_TYPE_STRING,
                                G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

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
	g_object_set (cell,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
	g_object_set (cell,
                "is-expanded", TRUE,
                NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Feed Name"),
                                                  cell,
                                                  "text", 1,
                                                  NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview),
                               column);
	gtk_tree_view_column_set_sort_column_id (column, 1);
	gtk_tree_view_column_clicked(column);
	column = gtk_tree_view_column_new_with_attributes (_("Type"),
                                                  cell,
                                                  "text", 2,
                                                  NULL);
//	gtk_tree_view_column_set_resizable(column, TRUE);
//	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 111);
//	gtk_tree_view_column_set_min_width (column, -1);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview),
                               column);
	gtk_tree_view_column_set_sort_column_id (column, 2);
	gtk_tree_view_set_search_column (GTK_TREE_VIEW (treeview),
                                                   2);
	gtk_tree_view_set_search_column(treeview, 1);
#if GTK_VERSION >= 2012000
	gtk_tree_view_set_tooltip_column (treeview, 3);
#endif

	if (rf->hr != NULL)
        	g_hash_table_foreach(rf->hrname, construct_list, store);

	//make sure something (first row) is selected
       selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
       gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, 0);
       gtk_tree_selection_select_iter(selection, &iter);
 
	gtk_tree_view_columns_autosize (treeview);
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
	sf->check4 = glade_xml_get_widget(sf->gui, "checkbutton4");
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
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->check4),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_SHOW_COMMENTS, NULL));
	g_signal_connect(sf->check4, 
		"clicked", 
		G_CALLBACK(start_check_cb), 
		GCONF_KEY_SHOW_COMMENTS);


#if (EVOLUTION_VERSION < 21900)		// include devel too

	/*first make the tab visible */
	g_object_set(glade_xml_get_widget(sf->gui, "label_HTML"),
					"visible", TRUE, 
					NULL);
	g_object_set(glade_xml_get_widget(sf->gui, "vbox_HTML"),
					"visible", TRUE, 
					NULL);
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

	switch (render) {
		case 10:
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			break;
		case 1: 
#ifndef HAVE_WEBKIT
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			break;
#endif
		case 2:
#ifndef HAVE_GECKO
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			break;
#endif
		default:
			g_printf("Selected render not supported! Failling back to default.\n");
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), render);

	}

	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo),
					renderer,
					set_sensitive,
					NULL, NULL);

#if !defined(HAVE_GECKO) && !defined (HAVE_WEBKIT)
	GtkWidget *label_webkit = glade_xml_get_widget(sf->gui, "label_webkits");
	gtk_label_set_text(GTK_LABEL(label_webkit), _("Note: In order to be able to use Mozilla (Firefox) or Apple Webkit \nas renders you need firefox or webkit devel package \ninstalled and evolution-rss should be recompiled to see those packages."));
	gtk_widget_show(label_webkit);
#endif
	g_signal_connect (combo, "changed", G_CALLBACK (render_engine_changed), NULL);
	gtk_widget_show(combo);
	gtk_box_pack_start(GTK_BOX(sf->combo_hbox), combo, FALSE, FALSE, 0);
#endif

#if (EVOLUTION_VERSION < 22200)		// include devel too
	/*first make the tab visible */
	g_object_set(glade_xml_get_widget(sf->gui, "label_HTML"),
					"visible", TRUE, 
					NULL);
	g_object_set(glade_xml_get_widget(sf->gui, "vbox_HTML"),
					"visible", TRUE, 
					NULL);
	/* Network tab */
	sf->use_proxy = glade_xml_get_widget(sf->gui, "use_proxy");
	sf->host_proxy = glade_xml_get_widget(sf->gui, "host_proxy");
	sf->port_proxy = glade_xml_get_widget(sf->gui, "port_proxy");
	sf->details = glade_xml_get_widget(sf->gui, "details");
	sf->proxy_details = glade_xml_get_widget(sf->gui, "http-proxy-details");

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sf->use_proxy),
        	gconf_client_get_bool(rss_gconf, GCONF_KEY_USE_PROXY, NULL));
	g_signal_connect(sf->use_proxy, "clicked", G_CALLBACK(start_check_cb), GCONF_KEY_USE_PROXY);
#endif

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
