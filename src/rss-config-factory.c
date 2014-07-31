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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301 USA
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
#if EVOLUTION_VERSION < 30304
#include <gconf/gconf-client.h>
#endif
#include <gdk/gdkkeysyms.h>

#if (DATASERVER_VERSION >= 2031001)
#include <camel/camel.h>
#else
#include <camel/camel-store.h>
#include <camel/camel-provider.h>
#endif

#include <mail/em-config.h>

#if EVOLUTION_VERSION <  30303
#include <mail/e-mail-local.h>
#endif

#include <mail/em-folder-selector.h>
#if EVOLUTION_VERSION >= 31102
#include <libemail-engine/libemail-engine.h>
#else
#if EVOLUTION_VERSION >= 30305
#include <libemail-engine/e-mail-folder-utils.h>
#else
#if EVOLUTION_VERSION >= 30101
#include <mail/e-mail-folder-utils.h>
#endif
#endif
#endif

#include <mail/em-utils.h>
#include <shell/e-shell.h>
#include <shell/e-shell-view.h>


#ifdef HAVE_LIBSOUP_GNOME
#include <libsoup/soup-gnome.h>
#include <libsoup/soup-gnome-features.h>
#endif

extern int rss_verbose_debug;
extern EShellView *rss_shell_view;

#include "rss.h"
#include "rss-formatter.h"
#include "misc.h"
#include "parser.h"
#include "rss-config.h"
#include "rss-config-factory.h"
#include "rss-evo-common.h"
#include "network-soup.h"
#include "notification.h"

GHashTable *tmphash = NULL;
static guint feed_enabled = 0;
static guint feed_validate = 0;
static guint feed_html = 0;
guint ccurrent = 0, ctotal = 0;
GList *flist = NULL;
gchar *strbuf;
GString *spacer = NULL;
GtkWidget *import_progress;
GtkWidget *import_dialog = NULL;

extern guint progress;
extern rssfeed *rf;
extern guint upgrade;
extern guint count;
extern gchar *buffer;
extern GSList *rss_list;
extern GHashTable *icons;
#if LIBSOUP_VERSION > 2024000
extern SoupCookieJar *rss_soup_jar;
#endif

#define RSS_CONTROL_ID  "OAFIID:GNOME_Evolution_RSS:" EVOLUTION_VERSION_STRING
#define FACTORY_ID      "OAFIID:GNOME_Evolution_RSS_Factory:" EVOLUTION_VERSION_STRING
#define MAX_TTL		10000

typedef struct {
	GtkBuilder  *xml;
	GtkWidget   *minfont;
	GtkWidget   *combobox;
	GtkWidget   *check;
	GtkWidget   *nettimeout;
	GtkWidget   *import;
} UIData;

void font_cb(GtkWidget *widget, GtkWidget *data);
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
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default();
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
#endif

	model = gtk_combo_box_get_model (dropdown);
	if (id == -1 || !gtk_tree_model_iter_nth_child (model, &iter, NULL, id))
		return;
	if (!id) id = 10;

#if EVOLUTION_VERSION < 30304
	gconf_client_set_int(client, GCONF_KEY_HTML_RENDER, id, NULL);
#else
	g_settings_set_int(settings, CONF_HTML_RENDER, id);
#endif
#ifdef HAVE_GECKO
	if (id == 2)
		rss_mozilla_init();
#endif
#if EVOLUTION_VERSION < 30304
	g_object_unref(client);
#else
	g_object_unref(settings);
#endif
}

static void
start_check_cb(GtkWidget *widget, gpointer data)
{
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default();
	gboolean active = gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (widget));
	/* Save the new setting to gconf */
	gconf_client_set_bool (client, data, active, NULL);
	g_object_unref(client);
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
	gboolean active = gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (widget));
	/* Save the new setting */
	g_settings_set_boolean (settings, data, active);
	g_object_unref(settings);
#endif
}

void
accept_cookies_cb(GtkWidget *widget, GtkWidget *data)
{
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default();
	gboolean active = gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (widget));
	/* Save the new setting to gconf */
	gconf_client_set_bool (
		client,
		GCONF_KEY_ACCEPT_COOKIES, active, NULL);
	gtk_widget_set_sensitive(data, active);
	g_object_unref(client);
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
	gboolean active = gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (widget));
	/* Save the new setting */
	g_settings_set_boolean (settings, CONF_ACCEPT_COOKIES, active);
	gtk_widget_set_sensitive(data, active);
	g_object_unref(settings);
#endif
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

	g_print("enable_toggle_cb()\n");
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, 0, &fixed, -1);
	gtk_tree_model_get (model, &iter, 3, &name, -1);
	fixed ^= 1;
	g_hash_table_replace(rf->hre,
		g_strdup(lookup_key(name)),
		GINT_TO_POINTER(fixed));
	gtk_list_store_set (
		GTK_LIST_STORE (model),
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
			GtkTreePath *path,
			GtkTreeViewColumn *column)
{
	feeds_dialog_edit((GtkDialog *)treeview, treeview);
}

static void
rep_check_cb (GtkWidget *widget, gpointer data)
{
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default();
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
#endif
	gboolean active =
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	/* Save the new setting to gconf */
#if EVOLUTION_VERSION < 30304
	gconf_client_set_bool (client,
		GCONF_KEY_REP_CHECK, active, NULL);
#else
	g_settings_set_boolean (settings,
		CONF_REP_CHECK, active);
#endif
	//if we already have a timeout set destroy it first
	if (rf->rc_id && !active)
		g_source_remove(rf->rc_id);
	if (active) {
			gtk_spin_button_update((GtkSpinButton *)data);
			//we have to make sure we have a timeout value
#if EVOLUTION_VERSION < 30304
			if (!gconf_client_get_float(client, GCONF_KEY_REP_CHECK_TIMEOUT, NULL))
				gconf_client_set_float (client,
					GCONF_KEY_REP_CHECK_TIMEOUT,
					gtk_spin_button_get_value((GtkSpinButton *)data),
					NULL);
#else
			if (!g_settings_get_double(settings, CONF_REP_CHECK_TIMEOUT))
				g_settings_set_double (settings,
					CONF_REP_CHECK_TIMEOUT,
					gtk_spin_button_get_value((GtkSpinButton *)data));
#endif
		if (rf->rc_id)
			g_source_remove(rf->rc_id);
		rf->rc_id = g_timeout_add (
				60 * 1000 * gtk_spin_button_get_value((GtkSpinButton *)data),
				(GSourceFunc) update_articles,
				(gpointer)1);
	}
#if EVOLUTION_VERSION < 30304
	g_object_unref(client);
#else
	g_object_unref(settings);
#endif
}

static void
enclosure_limit_cb (GtkWidget *widget, gpointer data)
{
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default();
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
#endif
	gboolean active =
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	/* Save the new setting to gconf */
#if EVOLUTION_VERSION < 30304
	gconf_client_set_bool (client,
		GCONF_KEY_ENCLOSURE_LIMIT,
		active,
		NULL);
#else
	g_settings_set_boolean (settings, CONF_ENCLOSURE_LIMIT, active);
#endif
	if (active) {
		//we have to make sure we have a timeout value
#if EVOLUTION_VERSION < 30304
		if (!gconf_client_get_float(client, GCONF_KEY_ENCLOSURE_SIZE, NULL))
				gconf_client_set_float (client,
					GCONF_KEY_ENCLOSURE_SIZE,
					gtk_spin_button_get_value((GtkSpinButton *)data),
					NULL);
#else
		if (!g_settings_get_double(settings, CONF_ENCLOSURE_SIZE))
				g_settings_set_double (settings,
					CONF_ENCLOSURE_SIZE,
					gtk_spin_button_get_value((GtkSpinButton *)data));
#endif
	}
#if EVOLUTION_VERSION < 30304
	g_object_unref(client);
#else
	g_object_unref(settings);
#endif
}

static void
enclosure_size_cb (GtkWidget *widget, gpointer data)
{
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default();
	gconf_client_set_float (client,
		GCONF_KEY_ENCLOSURE_SIZE,
		gtk_spin_button_get_value((GtkSpinButton*)widget),
		NULL);
	g_object_unref(client);
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
	g_settings_set_double (settings, CONF_ENCLOSURE_SIZE,
		gtk_spin_button_get_value((GtkSpinButton*)widget));
	g_object_unref(settings);
#endif
}

static void
rep_check_timeout_cb (GtkWidget *widget, gpointer data)
{
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default();
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
#endif
	gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data));
#if EVOLUTION_VERSION < 30304
	gconf_client_set_float (client,
		GCONF_KEY_REP_CHECK_TIMEOUT,
		gtk_spin_button_get_value((GtkSpinButton*)widget),
		NULL);
#else
	g_settings_set_double (settings, CONF_REP_CHECK_TIMEOUT,
		gtk_spin_button_get_value((GtkSpinButton*)widget));
#endif
	if (active) {
		if (rf->rc_id)
			g_source_remove(rf->rc_id);
		rf->rc_id = g_timeout_add (
			60 * 1000 * gtk_spin_button_get_value((GtkSpinButton *)widget),
			(GSourceFunc) update_articles,
			(gpointer)1);
	}
#if EVOLUTION_VERSION < 30304
	g_object_unref(client);
#else
	g_object_unref(settings);
#endif
}

static void
construct_list(gpointer key, gpointer value, gpointer user_data)
{
	GtkListStore  *store = user_data;
	GtkTreeIter    iter;
	gchar *full_name, *name, *full_path;

	gchar *tip = g_markup_escape_text(key, strlen(key));
	gtk_list_store_append (store, &iter);
	full_name = lookup_feed_folder_raw(key);
	name = g_path_get_basename(full_name);
	full_path = g_build_filename(
			lookup_main_folder(),
			full_name,
			NULL);
	gtk_list_store_set (
		store,
		&iter,
		0, g_hash_table_lookup(rf->hre, lookup_key(key)),
		1, name,
		2, g_hash_table_lookup(rf->hrt, lookup_key(key)),
		3, tip,
		4, full_path,
		-1);
	g_free(name);
	g_free(full_path);
	g_free(full_name);
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
update_days_label_cb (GtkWidget *widget, GtkLabel *data)
{
	guint val = gtk_spin_button_get_value((GtkSpinButton*)widget);
	gtk_label_set_text(data,
		ngettext("day", "days", val));
}

void
del_messages_cb (GtkWidget *widget, add_feed *data)
{
	guint adj = gtk_spin_button_get_value((GtkSpinButton*)widget);
	data->del_messages = adj;
}

void
update_messages_label_cb (GtkWidget *widget, GtkLabel *data)
{
	guint val = gtk_spin_button_get_value((GtkSpinButton*)widget);
	gtk_label_set_text(data,
		ngettext("message", "messages", val));
}

void
disable_widget_cb(GtkWidget *widget, GtkBuilder *data)
{
	GtkWidget *authuser = GTK_WIDGET (gtk_builder_get_object(data, "auth_user"));
	GtkWidget *authpass = GTK_WIDGET (gtk_builder_get_object(data, "auth_pass"));
	GtkToggleButton *useauth = (GtkToggleButton*)gtk_builder_get_object(data, "use_auth");
	gboolean auth_enabled = gtk_toggle_button_get_active(useauth);

	gtk_widget_set_sensitive(authuser, auth_enabled);
	gtk_widget_set_sensitive(authpass, auth_enabled);
}

void
folder_cb (GtkWidget *widget, gpointer data);

void
folder_cb (GtkWidget *widget, gpointer data)
{
	EMailBackend *backend;
	EMailSession *session;
#if EVOLUTION_VERSION >= 30101
	CamelStore *store = NULL;
	gchar *folder_name = NULL;
	const gchar *folderinfo;
#else
	CamelFolderInfo *folderinfo;
#endif
	GtkWidget *dialog;
	GtkWindow *window;
	const gchar *uri;
	struct _copy_folder_data *cfd;
#if EVOLUTION_VERSION >= 30101
	GError *error = NULL;
	EMFolderSelector *selector;
	EMFolderTree *folder_tree;
#if EVOLUTION_VERSION >= 30105
	EMFolderTreeModel *model;
#endif
#else
	GtkWidget *folder_tree;
#endif

	EMailReader *reader;
	EShellContent *shell_content;

	gchar *text = (gchar *)gtk_label_get_text(GTK_LABEL(data));

	shell_content = e_shell_view_get_shell_content (rss_shell_view);
	reader = E_MAIL_READER (shell_content);
	backend = e_mail_reader_get_backend (reader);

	session = e_mail_backend_get_session (backend);

	window = e_mail_reader_get_window (reader);

#if EVOLUTION_VERSION >= 30105
	model = em_folder_tree_model_get_default ();
#endif
#if EVOLUTION_VERSION >= 30303
#if EVOLUTION_VERSION >= 31301
	dialog = em_folder_selector_new (window, model);
	em_folder_selector_set_can_create (EM_FOLDER_SELECTOR (dialog), TRUE);
	em_folder_selector_set_caption (EM_FOLDER_SELECTOR (dialog), _("Move to Folder"));
	em_folder_selector_set_default_button_label (EM_FOLDER_SELECTOR (dialog), _("M_ove"));
#else
	dialog = em_folder_selector_new (
			window,
			model,
			EM_FOLDER_SELECTOR_CAN_CREATE,
			_("Move to Folder"), NULL, _("M_ove"));
#endif
#else
	dialog = em_folder_selector_new (
			window,
#if EVOLUTION_VERSION >= 30101
			backend,
#if EVOLUTION_VERSION >= 30105
			model,
#endif
#else
			EM_FOLDER_TREE (folder_tree),
#endif
			EM_FOLDER_SELECTOR_CAN_CREATE,
			_("Move to Folder"), NULL, _("M_ove"));
#endif

#if EVOLUTION_VERSION >= 30101
	selector = EM_FOLDER_SELECTOR (dialog);
	folder_tree = em_folder_selector_get_folder_tree (selector);
#else
	folder_tree = em_folder_tree_new (session);
	emu_restore_folder_tree_state (EM_FOLDER_TREE (folder_tree));
#endif

	em_folder_tree_set_excluded (
#if EVOLUTION_VERSION >= 30101
		folder_tree,
		EMFT_EXCLUDE_NOSELECT |
		EMFT_EXCLUDE_VIRTUAL |
#else
		EM_FOLDER_TREE (folder_tree),
		EMFT_EXCLUDE_NOSELECT | EMFT_EXCLUDE_VIRTUAL |
#endif
		EMFT_EXCLUDE_VTRASH);

	if ((uri = lookup_uri_by_folder_name(text)))
#if EVOLUTION_VERSION >= 30101
		em_folder_tree_set_selected (
			folder_tree, uri, FALSE);
#else
		em_folder_selector_set_selected (
			EM_FOLDER_SELECTOR (dialog),
			uri);
#endif

#if EVOLUTION_VERSION >= 30101
	folderinfo = em_folder_tree_get_selected_uri ((EMFolderTree *)folder_tree);
#else
	folderinfo = em_folder_tree_get_selected_folder_info ((EMFolderTree *)folder_tree);
#endif

	cfd = g_malloc (sizeof (*cfd));
#if EVOLUTION_VERSION < 30101
	cfd->fi = folderinfo;
#endif
	cfd->delete = 1;

#if EVOLUTION_VERSION >= 30101
	e_mail_folder_uri_parse (
		CAMEL_SESSION (session), folderinfo,
		&cfd->source_store, &cfd->source_folder_name, &error);

	if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		g_free (cfd);
		return;
	}
#endif

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		gchar *tmp;
		gchar *name = g_path_get_basename(text);
		uri = em_folder_selector_get_selected_uri (
			EM_FOLDER_SELECTOR (dialog));
		rss_emfu_copy_folder_selected (backend, uri, cfd);
#if EVOLUTION_VERSION >= 30101
		e_mail_folder_uri_parse (
			CAMEL_SESSION(session), uri,
			&store, &folder_name, NULL);
#endif
		tmp = g_build_path(G_DIR_SEPARATOR_S,
#if EVOLUTION_VERSION >= 30101
			folder_name,
#else
			em_utils_folder_name_from_uri(uri),
#endif
			name, NULL);
		g_free(name);
		gtk_label_set_text(GTK_LABEL(data), tmp);
		g_free(tmp);
	}

	gtk_widget_destroy (dialog);
}


add_feed *
build_dialog_add(gchar *url, gchar *feed_text)
{
	char *uifile;
	add_feed *feed = g_new0(add_feed, 1);
	GtkBuilder  *gui;
	gchar *flabel = NULL;
	gchar *fname;
	gboolean fhtml = FALSE;
	gboolean del_unread = FALSE,
		del_notpresent = FALSE;
	gboolean auth_enabled;
	guint del_feed = 0;
	gpointer key = NULL;
	GtkAccelGroup *accel_group = gtk_accel_group_new ();
	GtkWidget *ok, *cancel;
	GtkWidget *dialog1, *child;
	GtkWidget *authuser, *authpass;
	GtkWidget *adv_options, *entry1, *entry2, *feed_name;
	GtkToggleButton *useauth;
	GtkWidget *feed_name_label,
		*location_button,
		*location_label;
	GtkWidget *combobox1,
		*checkbutton1,
		*checkbutton2,
		*checkbutton3,
		*radiobutton1,
		*radiobutton2,
		*radiobutton3,
		*radiobutton4,
		*radiobutton5,
		*radiobutton6,
		*radiobutton7,
		*ttl_value;
	GtkWidget *label1, *label2;
	GtkWidget *spinbutton1, *spinbutton2;
	GtkWidget *checkbutton4;
	GtkImage *image;
	GError* error = NULL;

	feed->enabled = TRUE;
	uifile = g_build_filename (EVOLUTION_UIDIR,
		"rss-main.ui",
		NULL);
	gui = gtk_builder_new ();
	if (!gtk_builder_add_from_file (gui, uifile, &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}
	g_free (uifile);

	dialog1 = GTK_WIDGET (gtk_builder_get_object(gui, "feed_dialog"));
	child = GTK_WIDGET (gtk_builder_get_object(gui, "dialog-vbox9"));
//	gtk_widget_show(dialog1);
//	gtk_window_set_keep_above(GTK_WINDOW(dialog1), FALSE);
	if (url != NULL)
		gtk_window_set_title (GTK_WINDOW (dialog1), _("Edit Feed"));
	else
		gtk_window_set_title (GTK_WINDOW (dialog1), _("Add Feed"));
//	gtk_window_set_modal (GTK_WINDOW (dialog1), FALSE);

	adv_options = GTK_WIDGET (gtk_builder_get_object(gui, "adv_options"));
	entry1 = GTK_WIDGET (gtk_builder_get_object(gui, "url_entry"));
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
		del_notpresent = GPOINTER_TO_INT(
			g_hash_table_lookup(rf->hrdel_notpresent, key));
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

	entry2 = GTK_WIDGET (gtk_builder_get_object(gui, "entry2"));
	feed_name = GTK_WIDGET (gtk_builder_get_object(gui, "feed_name"));
	if (url != NULL) {
		gchar *folder_name = lookup_feed_folder(feed_text);
		flabel = g_build_path(G_DIR_SEPARATOR_S,
			lookup_main_folder(),
			folder_name,
			NULL);
		gtk_label_set_text(GTK_LABEL(entry2), flabel);
		fname = g_path_get_basename(folder_name);
		g_free(folder_name);
		gtk_entry_set_text(GTK_ENTRY(feed_name), fname);
		g_free(fname);
		gtk_widget_show(feed_name);
		feed_name_label = GTK_WIDGET (gtk_builder_get_object(gui, "feed_name_label"));
		gtk_widget_show(feed_name_label);
		location_button = GTK_WIDGET (gtk_builder_get_object(gui, "location_button"));

		gtk_widget_show(location_button);
		g_signal_connect (
			GTK_BUTTON (location_button),
			"clicked", G_CALLBACK (folder_cb), entry2);
		location_label = GTK_WIDGET (
			gtk_builder_get_object(gui,
			"location_label"));
		gtk_widget_show(location_label);
		gtk_label_set_use_markup(GTK_LABEL(entry2), 1);
	} else
		gtk_label_set_text(GTK_LABEL(entry2), flabel);

	combobox1 = GTK_WIDGET (gtk_builder_get_object(gui, "combobox1"));
	gtk_combo_box_set_active(
		GTK_COMBO_BOX(combobox1),
		0);

	checkbutton1 = GTK_WIDGET (gtk_builder_get_object(gui, "html_check"));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (checkbutton1),
		fhtml);

	checkbutton2 = GTK_WIDGET (gtk_builder_get_object(gui, "enabled_check"));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (checkbutton2),
		feed->enabled);

	checkbutton3 = GTK_WIDGET (gtk_builder_get_object(gui, "validate_check"));
	if (url)
		gtk_widget_set_sensitive(checkbutton3, FALSE);
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (checkbutton3),
		feed->validate);

	spinbutton1 = GTK_WIDGET (gtk_builder_get_object(gui, "storage_sb1"));
	spinbutton2 = GTK_WIDGET (gtk_builder_get_object(gui, "storage_sb2"));
	label1 = GTK_WIDGET(gtk_builder_get_object(gui, "label12"));
	g_signal_connect(
		spinbutton1,
		"value-changed",
		G_CALLBACK(update_messages_label_cb),
		label1);
	if (feed->del_messages)
		gtk_spin_button_set_value(
			GTK_SPIN_BUTTON(spinbutton1),
			feed->del_messages);
	g_signal_connect(
		spinbutton1,
		"changed",
		G_CALLBACK(del_messages_cb),
		feed);

	radiobutton1 = GTK_WIDGET (gtk_builder_get_object(gui, "storage_rb1"));
	radiobutton2 = GTK_WIDGET (gtk_builder_get_object(gui, "storage_rb2"));
	radiobutton3 = GTK_WIDGET (gtk_builder_get_object(gui, "storage_rb3"));
	radiobutton7 = GTK_WIDGET (gtk_builder_get_object(gui, "storage_rb4"));
	radiobutton4 = GTK_WIDGET (gtk_builder_get_object(gui, "ttl_global"));
	radiobutton5 = GTK_WIDGET (gtk_builder_get_object(gui, "ttl"));
	radiobutton6 = GTK_WIDGET (gtk_builder_get_object(gui, "ttl_disabled"));
	ttl_value = GTK_WIDGET (gtk_builder_get_object(gui, "ttl_value"));
	image = (GtkImage *)gtk_builder_get_object (gui, "image1");
	gtk_spin_button_set_range(
		(GtkSpinButton *)ttl_value,
		0,
		(guint)MAX_TTL);

	/*set feed icon*/
	if (key) {
		gtk_image_set_from_icon_name(
			image,
			g_hash_table_lookup(icons, key) ? key : "rss",
			GTK_ICON_SIZE_SMALL_TOOLBAR);
		gtk_widget_show(GTK_WIDGET(image));
	}

	switch (del_feed) {
	case 1:         //all but the last
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(radiobutton2),
			1);
		break;
	case 2:         //older than days
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(radiobutton3),
			1);
		break;
	default:
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(radiobutton1),
			1);
	}

	label2 = GTK_WIDGET(gtk_builder_get_object(gui, "label13"));
	g_signal_connect(
		spinbutton2,
		"value-changed",
		G_CALLBACK(update_days_label_cb),
		label2);
	if (feed->del_days)
		gtk_spin_button_set_value(
			GTK_SPIN_BUTTON(spinbutton2),
			feed->del_days);
	g_signal_connect(
		spinbutton2,
		"changed",
		G_CALLBACK(del_days_cb),
		feed);

	checkbutton4 = GTK_WIDGET (
		gtk_builder_get_object(gui,
		"storage_unread"));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (checkbutton4),
		del_unread);
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON(radiobutton7),
		del_notpresent);

	gtk_spin_button_set_value(
		GTK_SPIN_BUTTON(ttl_value),
		feed->ttl);
	g_signal_connect(
		ttl_value,
		"changed",
		G_CALLBACK(ttl_cb),
		feed);

	gtk_combo_box_set_active(
		GTK_COMBO_BOX(combobox1),
		feed->ttl_multiply);
	g_signal_connect(
		combobox1,
		"changed",
		G_CALLBACK(ttl_multiply_cb),
		feed);

	switch (feed->update) {
	case 2:
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (radiobutton5),
			1);
		break;
	case 3:
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (radiobutton6),
			1);
		break;
	default:
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (radiobutton4),
			1);
		break;
	}

	authuser = GTK_WIDGET (gtk_builder_get_object(gui, "auth_user"));
	authpass = GTK_WIDGET (gtk_builder_get_object(gui, "auth_pass"));
	useauth = (GtkToggleButton*)gtk_builder_get_object(gui, "use_auth");

	if (url && read_up(url)) {
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (useauth),
			1);
		gtk_entry_set_text(
			GTK_ENTRY(authuser),
			g_hash_table_lookup(rf->hruser, url));
		gtk_entry_set_text(
			GTK_ENTRY(authpass),
			g_hash_table_lookup(rf->hrpass, url));
	}
	auth_enabled = gtk_toggle_button_get_active(useauth);
	gtk_widget_set_sensitive(authuser, auth_enabled);
	gtk_widget_set_sensitive(authpass, auth_enabled);
	g_signal_connect(
		useauth,
		"toggled",
		G_CALLBACK(disable_widget_cb),
		gui);

	cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_widget_show (cancel);
	gtk_dialog_add_action_widget (GTK_DIALOG(dialog1),
		cancel, GTK_RESPONSE_CANCEL);

	ok = gtk_button_new_from_stock (GTK_STOCK_OK);
	gtk_widget_show (ok);
	gtk_dialog_add_action_widget (GTK_DIALOG(dialog1),
		ok, GTK_RESPONSE_OK);

	gtk_widget_add_accelerator (
		ok,
		"activate",
		accel_group,
		#if GTK_CHECK_VERSION(2,99,0)
		GDK_KEY_Return,
		#else
		GDK_Return,
		#endif
		(GdkModifierType) 0,
		GTK_ACCEL_VISIBLE);
	gtk_widget_add_accelerator (
		ok,
		"activate",
		accel_group,
		#if GTK_CHECK_VERSION(2,99,0)
		GDK_KEY_KP_Enter,
		#else
		GDK_KP_Enter,
		#endif
		(GdkModifierType) 0,
		GTK_ACCEL_VISIBLE);
	gtk_window_add_accel_group (
		GTK_WINDOW (dialog1),
		accel_group);

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
	GtkWidget *entry1 = GTK_WIDGET (
			gtk_builder_get_object(feed->gui, "url_entry"));
	GtkWidget *checkbutton1 = GTK_WIDGET (
			gtk_builder_get_object(feed->gui, "html_check"));
	GtkWidget *checkbutton2 = GTK_WIDGET (
			gtk_builder_get_object(feed->gui, "enabled_check"));
	GtkWidget *checkbutton3 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "validate_check"));
	GtkWidget *checkbutton4 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_unread"));
	GtkWidget *radiobutton1 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_rb1"));
	GtkWidget *radiobutton2 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_rb2"));
	GtkWidget *radiobutton3 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_rb3"));
	GtkWidget *radiobutton7 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_rb4"));
	GtkWidget *radiobutton4 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "ttl_global"));
	GtkWidget *radiobutton5 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "ttl"));
	GtkWidget *radiobutton6 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "ttl_disabled"));
	GtkWidget *spinbutton1 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_sb1"));
	GtkWidget *spinbutton2 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_sb2"));
	GtkWidget *ttl_value = GTK_WIDGET (gtk_builder_get_object (feed->gui, "ttl_value"));
	gboolean fhtml = feed->fetch_html;
	guint i=0;

	gint result = gtk_dialog_run(GTK_DIALOG(feed->dialog));
	switch (result) {
	case GTK_RESPONSE_OK:
		//grey out while were processing
		gtk_widget_set_sensitive(feed->dialog, FALSE);
		feed->feed_url = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(entry1)));
		fhtml = gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (checkbutton1));
		feed->fetch_html = fhtml;
		feed->enabled = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(checkbutton2));
		feed->validate = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(checkbutton3));
		while (i<4) {
			if (gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(radiobutton1)))
				break;
			i++;
			if (gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(radiobutton2)))
				break;
			i++;
			if (gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(radiobutton3)))
				break;
		}
		feed->del_feed = i;
		feed->del_unread = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(checkbutton4));
		feed->del_notpresent = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(radiobutton7));
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
	GtkTreeModel *model;

	if (!data)
		return FALSE;

	if (!store_redrawing) {
		store_redrawing = 1;
		model = gtk_tree_view_get_model (data);
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
	if (response == GTK_RESPONSE_CANCEL)
		rf->cancel = 1;
	gtk_widget_destroy(selector);
}

static void
feeds_dialog_add(GtkDialog *d, gpointer data)
{
	gchar *text;
	add_feed *feed = create_dialog_add(NULL, NULL);
	GtkWidget *msg_feeds, *progress;

	if (feed->dialog)
		gtk_widget_destroy(feed->dialog);
#if EVOLUTION_VERSION < 22904
	msg_feeds = e_error_new(
		GTK_WINDOW(rf->preferences),
		"org-gnome-evolution-rss:rssmsg",
		"",
		NULL);
#else
	msg_feeds = e_alert_dialog_new_for_args(
		GTK_WINDOW(rf->preferences),
		"org-gnome-evolution-rss:rssmsg",
		"",
		NULL);
#endif
	progress = gtk_progress_bar_new();
#if GTK_CHECK_VERSION (2,14,0)
	gtk_box_pack_start(
		GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(msg_feeds))),
		progress,
		FALSE,
		FALSE,
		0);
#else
	gtk_box_pack_start(
		GTK_BOX(GTK_DIALOG(msg_feeds)->vbox),
		progress,
		FALSE,
		FALSE,
		0);
#endif
	gtk_progress_bar_set_fraction(
		(GtkProgressBar *)progress,
		0);
	/* xgettext:no-c-format */
	gtk_progress_bar_set_text(
		(GtkProgressBar *)progress,
		_("0% done"));
	feed->progress = progress;
	gtk_window_set_keep_above(
		GTK_WINDOW(msg_feeds),
		TRUE);
	g_signal_connect(
		msg_feeds,
		"response",
		G_CALLBACK(msg_feeds_response),
		NULL);
	gtk_widget_show_all(msg_feeds);
	if (feed->feed_url && strlen(feed->feed_url)) {
		text = feed->feed_url;
		feed->feed_url = sanitize_url(feed->feed_url);
		g_free(text);
		if (g_hash_table_find(rf->hr,
			check_if_match,
			feed->feed_url)) {
				rss_error(
					NULL,
					NULL,
					_("Error adding feed."),
					_("Feed already exists!"));
			goto out;
		}
		setup_feed(feed);
	}
out:    d("msg_feeds destroy\n");
	gtk_widget_destroy(msg_feeds);
	feed->progress = NULL;
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
rss_delete_rec (CamelStore *store, CamelFolderInfo *fi, GError **error)
{
	int i;
	GPtrArray *uids;

	while (fi) {
		CamelFolder *folder;

		d("deleting folder '%s'\n", fi->full_name);

#if (DATASERVER_VERSION >= 2033001)
		if (!(folder = camel_store_get_folder_sync (store, fi->full_name,
			0, NULL, error)))
#else
		if (!(folder = camel_store_get_folder (store, fi->full_name,
			0, error)))
#endif
			return;

			uids = camel_folder_get_uids (folder);

			camel_folder_freeze (folder);
			for (i = 0; i < uids->len; i++)
				camel_folder_delete_message (
					folder,
					uids->pdata[i]);

			camel_folder_free_uids (folder, uids);

#if (DATASERVER_VERSION >= 2033001)
			camel_folder_synchronize_sync (folder, TRUE, NULL, NULL);
#else
			camel_folder_sync (folder, TRUE, NULL);
#endif
			camel_folder_thaw (folder);

		d("do camel_store_delete_folder()\n");

#if (DATASERVER_VERSION >= 2033001)
		camel_store_delete_folder_sync (store, fi->full_name, NULL, error);
#else
		camel_store_delete_folder (store, fi->full_name, error);
#endif
		if (error != NULL)
			return;

		fi = fi->next;
	}
}

void
rss_delete_folders (CamelStore *store,
		const char *full_name,
		GError **error)
{
	guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE
		| CAMEL_STORE_FOLDER_INFO_FAST
		| CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
	CamelFolderInfo *fi = NULL;

	d("camel_store_get_folder_info() %s\n", full_name);
#if (DATASERVER_VERSION >= 2033001)
	fi = camel_store_get_folder_info_sync (
		store,
		full_name,
		flags, NULL, error);
#else
	fi = camel_store_get_folder_info (
		store,
		full_name,
		flags, error);
#endif
	if (!fi || *error != NULL)
		return;

	d("call rss_delete_rec()\n");
	rss_delete_rec (store, fi, error);
#if (DATASERVER_VERSION >= 3011001)
	camel_folder_info_free (fi);
#else
	camel_store_free_folder_info (store, fi);
#endif
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
	saved_feed->hrname =
		g_strdup(g_hash_table_lookup(rf->hrname, name));
	saved_feed->hrname_r =
		g_strdup(g_hash_table_lookup(
				rf->hrname_r, lookup_key(name)));
	saved_feed->hre =
		GPOINTER_TO_INT(g_hash_table_lookup(
					rf->hre, lookup_key(name)));
	saved_feed->hrt =
		g_strdup(g_hash_table_lookup(
				rf->hrt, lookup_key(name)));
	saved_feed->hrh =
		GPOINTER_TO_INT(g_hash_table_lookup(
					rf->hrh, lookup_key(name)));
	saved_feed->hr =
		g_strdup(g_hash_table_lookup(
				rf->hr,
				lookup_key(name)));
	saved_feed->hrdel_feed =
		GPOINTER_TO_INT(g_hash_table_lookup(
					rf->hrdel_feed,
					lookup_key(name)));
	saved_feed->hrdel_days =
		GPOINTER_TO_INT(g_hash_table_lookup(
					rf->hrdel_days,
					lookup_key(name)));
	saved_feed->hrdel_messages =
		GPOINTER_TO_INT(g_hash_table_lookup(
					rf->hrdel_messages,
					lookup_key(name)));
	saved_feed->hrdel_unread =
		GPOINTER_TO_INT(g_hash_table_lookup(
				rf->hrdel_unread, lookup_key(name)));
	saved_feed->hrdel_notpresent =
		GPOINTER_TO_INT(g_hash_table_lookup(
				rf->hrdel_notpresent, lookup_key(name)));
	saved_feed->hrupdate =
		GPOINTER_TO_INT(g_hash_table_lookup(
				rf->hrupdate, lookup_key(name)));
	saved_feed->hrttl =
		GPOINTER_TO_INT(g_hash_table_lookup(
				rf->hrttl, lookup_key(name)));
	saved_feed->hrttl_multiply =
		GPOINTER_TO_INT(g_hash_table_lookup(
				rf->hrttl_multiply,
				lookup_key(name)));
	return saved_feed;
}

// restores a feed structure removed from hash
// name - key to restore
// s - feed structure to restore
// upon return s structure is destroyed
void
restore_feed_hash(hrfeed *s)
{
	g_hash_table_insert(
		rf->hrname, s->hrname_r, s->hrname);
	g_hash_table_insert(
		rf->hrname_r, s->hrname, s->hrname_r);
	g_hash_table_insert(
		rf->hre,
		g_strdup(s->hrname),
		GINT_TO_POINTER(s->hre));
	g_hash_table_insert(
		rf->hrh,
		g_strdup(s->hrname),
		GINT_TO_POINTER(s->hrh));
	g_hash_table_insert(
		rf->hrt,
		g_strdup(s->hrname),
		GINT_TO_POINTER(s->hrt));
	g_hash_table_insert(
		rf->hr, g_strdup(s->hrname), s->hr);
	g_hash_table_insert(
		rf->hrdel_feed,
		g_strdup(s->hrname),
		GINT_TO_POINTER(s->hrdel_feed));
	g_hash_table_insert(
		rf->hrdel_days,
		g_strdup(s->hrname),
		GINT_TO_POINTER(s->hrdel_days));
	g_hash_table_insert(
		rf->hrdel_messages,
		g_strdup(s->hrname),
		GINT_TO_POINTER(s->hrdel_messages));
	g_hash_table_insert(
		rf->hrdel_unread,
		g_strdup(s->hrname),
		GINT_TO_POINTER(s->hrdel_unread));
	g_hash_table_insert(
		rf->hrdel_notpresent,
		g_strdup(s->hrname),
		GINT_TO_POINTER(s->hrdel_notpresent));
	g_hash_table_insert(
		rf->hrupdate,
		g_strdup(s->hrname),
		GINT_TO_POINTER(s->hrupdate));
	g_hash_table_insert(
		rf->hrttl,
		g_strdup(s->hrname),
		GINT_TO_POINTER(s->hrttl));
	g_hash_table_insert(
		rf->hrttl_multiply,
		g_strdup(s->hrname),
		GINT_TO_POINTER(s->hrttl_multiply));
	g_free(s);
	g_hash_table_destroy(rf->feed_folders);
	g_hash_table_destroy(rf->reversed_feed_folders);
	get_feed_folders();
}

void
remove_feed_hash(gpointer name)
{
	//we need to make sure we won't fetch_feed iterate over those hashes
	rf->pending = TRUE;
	/* FIXED since we force cancel_all msg
	 * check on normal feed deletion while fetching
	 */
//	taskbar_op_finish(name);
	g_hash_table_remove(rf->hre, lookup_key(name));
	g_hash_table_remove(rf->hrt, lookup_key(name));
	g_hash_table_remove(rf->hrh, lookup_key(name));
	g_hash_table_remove(rf->hr, lookup_key(name));
	g_hash_table_remove(rf->hrdel_feed, lookup_key(name));
	g_hash_table_remove(rf->hrdel_days, lookup_key(name));
	g_hash_table_remove(rf->hrdel_messages, lookup_key(name));
	g_hash_table_remove(rf->hrdel_unread, lookup_key(name));
	g_hash_table_remove(rf->hrdel_notpresent, lookup_key(name));
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
	gchar *feed_dir = rss_component_peek_base_directory();
	gchar *feed_file, *orig_name;

	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);
	feed_file = g_strdup_printf("%s/feed_folders", feed_dir);
	g_free(feed_dir);
	f = fopen(feed_file, "wb");
	if (!f) {
		g_free(feed_file);
		return;
	}

	orig_name = g_hash_table_lookup(
			rf->feed_folders,
			old_name);
	if (orig_name)
		g_hash_table_remove(
			rf->feed_folders,
			old_name);

	g_hash_table_foreach(
		rf->feed_folders,
		(GHFunc)write_feeds_folder_line,
		(gpointer *)f);
	fclose(f);
	g_hash_table_destroy(rf->reversed_feed_folders);
	rf->reversed_feed_folders =
		g_hash_table_new_full(
			g_str_hash,
			g_str_equal,
			g_free,
			g_free);
	g_hash_table_foreach(
		rf->feed_folders,
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
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default();
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
#endif
	if (response == GTK_RESPONSE_OK) {
		selection =
			gtk_tree_view_get_selection(
				GTK_TREE_VIEW(user_data));
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get (
				model, &iter,
				4, &name,
				-1);
			rss_delete_feed(name,
#if EVOLUTION_VERSION < 30304
				gconf_client_get_bool(
					client,
					GCONF_KEY_REMOVE_FOLDER,
					NULL));
#else
				g_settings_get_boolean(settings, CONF_REMOVE_FOLDER));
#endif
			g_free(name);
		}
		store_redraw(GTK_TREE_VIEW(rf->treeview));
		save_gconf_feed();
	}
	gtk_widget_destroy(selector);
	rf->import = 0;
#if EVOLUTION_VERSION < 30304
	g_object_unref(client);
#else
	g_object_unref(settings);
#endif
}

void
feeds_dialog_disable(GtkDialog *d, gpointer data)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	gchar *name;
	gpointer key;

	selection =
		gtk_tree_view_get_selection(
			GTK_TREE_VIEW(rf->treeview));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, 3, &name, -1);
		key = lookup_key(name);
		g_free(name);
		g_hash_table_replace(
			rf->hre,
			g_strdup(key),
			GINT_TO_POINTER(
				!g_hash_table_lookup(rf->hre, key)));
		gtk_button_set_label(
			data,
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
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default();
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
#endif

#if EVOLUTION_VERSION < 22904
	dialog1 = e_error_new(
		GTK_WINDOW(rf->preferences),
		"org-gnome-evolution-rss:ask-delete-feed",
		msg,
		NULL);
#else
	dialog1 = e_alert_dialog_new_for_args(
		GTK_WINDOW(rf->preferences),
		"org-gnome-evolution-rss:ask-delete-feed",
		msg,
		NULL);
#endif
	gtk_window_set_keep_above(GTK_WINDOW(dialog1), TRUE);

#if GTK_CHECK_VERSION (2,14,0)
	dialog_vbox1 = gtk_dialog_get_content_area(GTK_DIALOG (dialog1));
#else
	dialog_vbox1 = GTK_DIALOG (dialog1)->vbox;
#endif
	gtk_widget_show (dialog_vbox1);

#if GTK_MAJOR_VERSION < 3
	vbox1 = gtk_vbox_new (FALSE, 10);
#else
	vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
#endif
	gtk_widget_show (vbox1);
	gtk_box_pack_start (
		GTK_BOX (dialog_vbox1),
		vbox1,
		TRUE,
		TRUE,
		0);
	gtk_container_set_border_width (
		GTK_CONTAINER (vbox1),
		10);

	checkbutton1 = gtk_check_button_new_with_mnemonic (
			_("Remove folder contents"));
	gtk_widget_show (checkbutton1);
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (checkbutton1),
#if EVOLUTION_VERSION < 30304
	gconf_client_get_bool(
		client,
		GCONF_KEY_REMOVE_FOLDER, NULL));
#else
	g_settings_get_boolean(settings, CONF_REMOVE_FOLDER));
#endif
	g_signal_connect(
		checkbutton1,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_REMOVE_FOLDER);
#else
		(gpointer)CONF_REMOVE_FOLDER);
#endif
	gtk_box_pack_start (
		GTK_BOX (vbox1),
		checkbutton1,
		FALSE,
		FALSE,
		0);

#if GTK_CHECK_VERSION (2,14,0)
	dialog_action_area1 = gtk_dialog_get_action_area(GTK_DIALOG (dialog1));
#else
	dialog_action_area1 = GTK_DIALOG (dialog1)->action_area;
#endif
	gtk_widget_show (dialog_action_area1);
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (dialog_action_area1),
		GTK_BUTTONBOX_END);
#if EVOLUTION_VERSION < 30304
	g_object_unref(client);
#else
	g_object_unref(settings);
#endif
	return dialog1;
}

void
feeds_dialog_delete(GtkDialog *d, gpointer data)
{
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	GtkWidget *rfd;
	gchar *name;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)
		&& !rf->import) {
			rf->import = 1;
			gtk_tree_model_get (
				model,
				&iter,
				3,
				&name,
				-1);
			rfd = remove_feed_dialog(name);
			gtk_widget_show(rfd);
			g_signal_connect(
				rfd,
				"response",
				G_CALLBACK(delete_response),
				data);
			g_signal_connect(
				rfd,
				"destroy",
				G_CALLBACK(destroy_delete),
				rfd);
			g_free(name);
	}
}

void
process_dialog_edit(add_feed *feed, gchar *url, gchar *feed_name)
{
	gchar *text = NULL;
	gpointer key = lookup_key(feed_name);
	gchar *prefix = NULL;
	hrfeed *saved_feed;
	GError *error = NULL;
	CamelStore *store = rss_component_peek_local_store();
	GtkWidget *msg_feeds, *progress;

#if EVOLUTION_VERSION < 22904
	msg_feeds = e_error_new(
		GTK_WINDOW(rf->preferences),
		"org-gnome-evolution-rss:rssmsg",
		"",
		NULL);
#else
	msg_feeds = e_alert_dialog_new_for_args(
		GTK_WINDOW(rf->preferences),
		"org-gnome-evolution-rss:rssmsg",
		"",
		NULL);
#endif
	progress = gtk_progress_bar_new();
#if GTK_CHECK_VERSION (2,14,0)
	gtk_box_pack_start(
		GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(msg_feeds))),
		progress,
		FALSE,
		FALSE,
		0);
#else
	gtk_box_pack_start(
		GTK_BOX(GTK_DIALOG(msg_feeds)->vbox),
		progress,
		FALSE,
		FALSE,
		0);
#endif
	gtk_progress_bar_set_fraction((GtkProgressBar *)progress, 0);
	/* xgettext:no-c-format */
	gtk_progress_bar_set_text((GtkProgressBar *)progress, _("0% done"));
	feed->progress=progress;
	gtk_window_set_keep_above(GTK_WINDOW(msg_feeds), TRUE);
	g_signal_connect(
		msg_feeds,
		"response",
		G_CALLBACK(msg_feeds_response),
		NULL);
	gtk_widget_show_all(msg_feeds);
	if (!feed->add)
		goto out;
	text = feed->feed_url;
	feed->feed_url = sanitize_url(feed->feed_url);
	g_free(text);
	if (feed->feed_url) {
		gchar *folder_name;
		feed->edit=1;
		folder_name = lookup_feed_folder(feed_name);
		prefix = g_path_get_dirname(folder_name);
		g_free(folder_name);
		if (*prefix != '.')
			feed->prefix = prefix;
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
			saved_feed = save_feed_hash(feed_name);
			remove_feed_hash(feed_name);
			feed->ok = (GFunc)destroy_feed_hash_content;
			feed->ok_arg = saved_feed;
			feed->cancelable = (GFunc)restore_feed_hash;
			feed->cancelable_arg = saved_feed;
			setup_feed(feed);
			/* move destory after finish_setup_feed */
			gtk_widget_destroy(msg_feeds);
			return;
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
				gchar *folder_name = lookup_feed_folder(feed_name);
				gchar *a = g_build_path(G_DIR_SEPARATOR_S,
					lookup_main_folder(),
					folder_name,
					NULL);
				gchar *dir = g_path_get_dirname(a);
				gchar *b = g_build_path(
						G_DIR_SEPARATOR_S,
						dir, feed->feed_name, NULL);
				g_free(folder_name);
#if (DATASERVER_VERSION >= 2033001)
				camel_store_rename_folder_sync (store, a, b, NULL, &error);
#else
				camel_store_rename_folder (store, a, b, &error);
#endif
				if (error != NULL) {
					e_alert_run_dialog_for_args(
						GTK_WINDOW(rf->preferences),
						"mail:no-rename-folder",
						a, b, error->message, NULL);
					g_clear_error(&error);
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
			g_hash_table_replace(
					rf->hrupdate,
					g_strdup(key),
					GINT_TO_POINTER(feed->update));
			g_hash_table_replace(
					rf->hrdel_unread,
					g_strdup(key),
					GINT_TO_POINTER(feed->del_unread));
			g_hash_table_replace(
					rf->hrdel_notpresent,
					g_strdup(key),
					GINT_TO_POINTER(feed->del_notpresent));
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
	gchar *tmp_feed_name;
	gpointer key;
	add_feed *feed = NULL;

	/* This will only work in single or browse selection mode! */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get (
			model,
			&iter,
			3, &feed_name,
			-1);
		/* seems we get the data from gtk_tree with html entities already translated
		 * so instead of adding versioned defs we fallback to decoding html entities
		 * in case key is not found, and in case that fails too we exit gracefully
		 */
		if (!(key = lookup_key(feed_name))) {
			tmp_feed_name = feed_name;
			feed_name = decode_entities(feed_name);
			g_free(tmp_feed_name);
			key = lookup_key(feed_name);
		}
		if (key) {
			if (name = g_hash_table_lookup(rf->hr, key)) {
				feed = create_dialog_add(name, feed_name);
				if (feed->dialog)
					gtk_widget_destroy(feed->dialog);
				process_dialog_edit(feed, name, feed_name);
			}
		}
		if (feed && feed->feed_url)
			store_redraw(GTK_TREE_VIEW(rf->treeview));
	}
}

void
import_dialog_response(
	GtkWidget *selector, guint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_CANCEL) {
		gtk_widget_destroy(rf->progress_dialog);
		rf->import_cancel = 1;
		rf->display_cancel = 1;
		progress = 0;
		//rf->cancel_all = 1;
		abort_all_soup();
	}
}

void
import_one_feed(gchar *url, gchar *title, gchar *prefix)
{
	gchar *tmp;
	add_feed *feed = g_new0(add_feed, 1);
	feed->changed=0;
	feed->add=1;
	feed->fetch_html = feed_html;
	feed->validate = feed_validate;
	feed->enabled = feed_enabled;
	feed->feed_url = g_strdup(url);
	tmp = decode_html_entities(title);
	if (strlen(tmp) > 40) {
		gchar *t = tmp;
		tmp = g_strndup(tmp, 40);
		g_free(t);
	}
	feed->feed_name = sanitize_folder(tmp);
	g_free(tmp);
	feed->prefix = g_strdup(prefix);
	rf->progress_bar = import_progress;
	rf->progress_dialog = import_dialog;
	if ((g_hash_table_find(rf->hr, check_if_match,feed->feed_url))
	   || (g_hash_table_find(tmphash, check_if_match, feed->feed_url))) {
		rss_error(
			title,
			feed->feed_name,
			_("Error adding feed."),
			_("Feed already exists!"));
		rf->import--;
	} else {
		setup_feed(feed);
		g_hash_table_insert(tmphash,
			g_strdup(url),
			g_strdup(url));
	}

	/* this allows adding feeds somewhat synchronous way
	 * it is very convenient to be able to cancel importing
	 * of a few hundred feeds
	 */
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
		src = html_find(src, (gchar *)"outline");
		*url = (gchar *)xmlGetProp(src, (xmlChar *)"xmlUrl");
		*title = xmlGetProp(src, (xmlChar *)"title");
		if (!(*title = xmlGetProp(src, (xmlChar *)"title")))
			*title = xmlGetProp(src, (xmlChar *)"text");
	} else if (type == 1) {
		xmlNode *my;
		src = html_find(src, (gchar *)"member");
		my = layer_find_pos(src, "member", "Agent");
		*title = xmlCharStrdup(layer_find(my, "name", NULL));
		my =  html_find(my, (gchar *)"Document");
		*url =  (gchar *)xmlGetProp(my, (xmlChar *)"about");
		if (!*url) {
			my =  html_find(my, (gchar *)"channel");
			*url =  (gchar *)xmlGetProp(my, (xmlChar *)"about");
		}
	}
	return src;
}

void import_opml(gchar *file);

void
import_opml(gchar *file)
{
	gchar *url = NULL;
	xmlChar *name = NULL;
	guint total = 0;
	guint type = 0; //file type
	gchar *msg, *tmp, *maintitle = NULL;
	GtkWidget *import_label;

	xmlNode *src = (xmlNode *)xmlParseFile (file);
	xmlNode *doc = NULL;

	if (!src) {
error:		rss_error(NULL,
			NULL,
			_("Import error."),
			_("Invalid file or this file does not contain any feeds."));
		goto out;
	}
	doc = src;
	tmp = g_path_get_basename(file);
	msg = g_strdup_printf("%s: %s", _("Importing"), tmp);
	g_free(tmp);
#if EVOLUTION_VERSION < 22904
	import_dialog = e_error_new(
		GTK_WINDOW(rf->preferences),
		"shell:importing",
		msg,
		NULL);
#else
	import_dialog = e_alert_dialog_new_for_args(
		GTK_WINDOW(rf->preferences),
		"shell:importing",
		msg,
		NULL);
#endif
	gtk_window_set_keep_above(
		GTK_WINDOW(import_dialog),
		TRUE);
	g_signal_connect(
		import_dialog,
		"response",
		G_CALLBACK(import_dialog_response),
		NULL);
	import_label = gtk_label_new(_("Please wait"));
	import_progress = gtk_progress_bar_new();
#if GTK_CHECK_VERSION (2,14,0)
	gtk_box_pack_start(
		GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(import_dialog))),
		import_label,
		FALSE,
		FALSE,
		0);
	gtk_box_pack_start(
		GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(import_dialog))),
		import_progress,
		FALSE,
		FALSE,
		0);
#else
	gtk_box_pack_start(
		GTK_BOX(GTK_DIALOG(import_dialog)->vbox),
		import_label,
		FALSE,
		FALSE,
		0);
	gtk_box_pack_start(
		GTK_BOX(GTK_DIALOG(import_dialog)->vbox),
		import_progress,
		FALSE,
		FALSE,
		0);
#endif
	gtk_widget_show_all(import_dialog);
	g_free(msg);
	if ((src = src->children)) {
		if (!g_ascii_strcasecmp((char *)src->name, "rdf")) {
			while (src) {
				src=src->children;
				src = src->next;
				src = src->children;
				d("group name:%s\n",
					layer_find(src, "name", NULL));
				src = src->next;
				while ((src = iterate_import_file(src, &url, &name, 1))) {
					if (url) {
						total++;
						xmlFree(url);
					}
					if (name) xmlFree(name);
				}
			d("total:%d\n", total);
			type = 1;
			}
		} else if (!g_ascii_strcasecmp((char *)src->name, "opml")) {
			while ((src = iterate_import_file(src, &url, &name, 0))) {
				if (url && strlen(url)) {
					total++;
					xmlFree(url);
				}
				if (name) xmlFree(name);
			}
			type = 0;
			d("total:%d\n", total);
		}
	}
	d("file type=%d\n", type);
	g_object_set_data((GObject *)import_progress, "total", GINT_TO_POINTER(total));
	g_object_set_data((GObject *)import_progress, "label", import_label);
	src = doc;
	name = NULL;
	if (type == 1) {
		src=src->children;
		d("my cont:%s\n", src->content);
		src = src->children;
		src = src->next;
		d("found %s\n", src->name);
		src = src->children;
		d("group name:%s\n", layer_find(src, "name", NULL));
		src = src->next;
	}

	if (type == 0) {
	gint size = 0;
	gchar *base = NULL, *root = NULL, *last = NULL;
	gchar *rssprefix = NULL;
	/* need to automate this, not just guess title at random */
	if (!src || !src->children || !src->children->children)
		goto error;
	src = src->children->children;
	if (!src->next)
		goto error;
	src = src->next;
	if (!src->children)
		goto error;
	src = src->children;
	maintitle = (gchar *)layer_find(src, "title", NULL);
	rf->import=2;
	if (!tmphash)
		tmphash = g_hash_table_new_full(
				g_str_hash,
				g_str_equal,
				g_free,
				g_free);
	progress = 0;
	rf->display_cancel=0; //clean this signal - as by this time we already cancel all displaying feed
	while (src) {
		gchar *rssurl = NULL, *rsstitle = NULL;
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
				if (root
				&& (last = g_path_get_basename(root))
				&& strcmp(last, ".")) {
					g_print("retract:%s\n", last);
					size = strstr(root, last)-root-1;
					tmp = root;
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
			gchar *prop = (gchar *)xmlGetProp(
						src,
						(xmlChar *)"type");
//			if (prop) {
				if ((prop && !strcmp(prop, "folder")) || !prop) {
					base = (gchar *)xmlGetProp(
							src,
							(xmlChar *)"text");
					if (NULL != src->last) {
						gchar *tmp = root;
						if (!root)
							root = g_build_path(
								G_DIR_SEPARATOR_S,
								base,
								NULL);
						else {
							root = g_build_path(
								G_DIR_SEPARATOR_S,
								root,
								base,
								NULL);
						}
						if (base) xmlFree(base);
						if (tmp) g_free(tmp);
					}
				// we're insterested in rss/pie only
				} else if (strcmp(prop, "link")) {
				//	&& strcmp(prop, "vfolder")) {
					if (maintitle)
						rssprefix = g_build_path(
								G_DIR_SEPARATOR_S,
								maintitle,
								root,
								NULL);
					else
						rssprefix = g_strdup(root);
					rssurl = (gchar *)xmlGetProp(
							src,
							(xmlChar *)"xmlUrl");
					if (!rssurl)
						goto fail;

					if (rf->import_cancel) {
						rf->import = 0;
						goto out;
					}
					rsstitle = (gchar *)xmlGetProp(
							src,
							(xmlChar *)"title");

					d("rssprefix:%s|rssurl:%s|rsstitle:%s|\n",
						rssprefix,
						rssurl, rsstitle);
					rf->import++;
					if (rf->import == 10) {
//					while(gtk_events_pending())
//					gtk_main_iteration();
					}
					import_one_feed(
						rssurl,
						rsstitle,
						rssprefix);

					if (rf->import_cancel) {
						rf->import = 0;
						goto out;
					}
					if (rssurl) xmlFree(rssurl);
					if (rsstitle) xmlFree(rsstitle);
fail:					g_free(rssprefix);
				}
			xmlFree(prop);
//			}
		}
	}
		goto out;
	}
	g_print("MARK #1\n");

	while ((src = iterate_import_file(src, &url, &name, type))) {
		if (url && strlen(url)) {
			d("url:%s\n", url);
			if (rf->cancel) {
				if (src) xmlFree(src);
				rf->cancel = 0;
				goto out;
			}
			gtk_label_set_text(
				GTK_LABEL(import_label),
				(gchar *)name);
#if GTK_CHECK_VERSION (2,6,0)
			gtk_label_set_ellipsize (
				GTK_LABEL (import_label),
				PANGO_ELLIPSIZE_START);
#endif
			gtk_label_set_justify(
				GTK_LABEL(import_label),
				GTK_JUSTIFY_CENTER);
			import_one_feed(url, (gchar *)name, NULL);
			rf->import++;
			if (name) xmlFree(name);
			if (url) xmlFree(url);

		}
	}
out:	g_hash_table_destroy(tmphash);
	tmphash=NULL;
	//prevent reseting queue before its time dues do async operations
	if (rf->import) rf->import -= 2;
	rf->import_cancel = 0;
	if (maintitle) xmlFree(maintitle);
	if (doc) xmlFree(doc);
	if (import_dialog) {
		gtk_widget_destroy(import_dialog);
		import_dialog = NULL;
	}
}

static void
select_file_response(
	GtkWidget *selector, guint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_OK) {
		char *name = gtk_file_chooser_get_filename (
				GTK_FILE_CHOOSER (selector));
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
	feed_html  = 1-gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (widget));
}

static void
import_toggle_cb_valid (GtkWidget *widget, gpointer data)
{
	feed_validate  = gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (widget));
}

static void
import_toggle_cb_ena (GtkWidget *widget, gpointer data)
{
	feed_enabled  = gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (widget));
}

static void
decorate_import_fs (gpointer data)
{
	GtkFileFilter *file_filter = gtk_file_filter_new ();
	GtkFileFilter *filter = gtk_file_filter_new ();
	GtkWidget *vbox1;
	GtkWidget *checkbutton1;
	GtkWidget *checkbutton2;
	GtkWidget *checkbutton3;

	gtk_file_chooser_set_do_overwrite_confirmation (
		GTK_FILE_CHOOSER (data),
		TRUE);
	gtk_dialog_set_default_response (
		GTK_DIALOG (data),
		GTK_RESPONSE_OK);
	gtk_file_chooser_set_local_only (data, FALSE);

	gtk_file_filter_add_pattern (
		GTK_FILE_FILTER(file_filter), "*");
	gtk_file_filter_set_name (
		GTK_FILE_FILTER(file_filter),
		_("All Files"));
	gtk_file_chooser_add_filter (
		GTK_FILE_CHOOSER (data),
		GTK_FILE_FILTER(file_filter));

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (
		GTK_FILE_FILTER(file_filter),
		"*.opml");
	gtk_file_filter_set_name (
		GTK_FILE_FILTER(file_filter),
		_("OPML Files"));
	gtk_file_chooser_add_filter (
		GTK_FILE_CHOOSER (data),
		GTK_FILE_FILTER(file_filter));

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (
		GTK_FILE_FILTER(file_filter),
		"*.xml");
	gtk_file_filter_set_name (
		GTK_FILE_FILTER(file_filter),
		_("XML Files"));
	gtk_file_chooser_add_filter (
		GTK_FILE_CHOOSER (data),
		GTK_FILE_FILTER(file_filter));

	gtk_file_chooser_set_filter (
		GTK_FILE_CHOOSER (data),
		GTK_FILE_FILTER(file_filter));

	gtk_file_filter_add_pattern (filter, "*.opml");
	gtk_file_filter_add_pattern (filter, "*.xml");
	gtk_file_chooser_set_filter(data, filter);

#if GTK_MAJOR_VERSION < 3
	vbox1 = gtk_vbox_new (FALSE, 0);
#else
	vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
#endif
	checkbutton1 = gtk_check_button_new_with_mnemonic (
			_("Show article's summary"));
	gtk_widget_show (checkbutton1);
	gtk_box_pack_start (
		GTK_BOX (vbox1),
		checkbutton1,
		FALSE,
		TRUE,
		0);
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (checkbutton1),
		1);

	checkbutton2 = gtk_check_button_new_with_mnemonic (
			_("Feed Enabled"));
	gtk_widget_show (checkbutton2);
	gtk_box_pack_start (
		GTK_BOX (vbox1),
		checkbutton2,
		FALSE,
		TRUE,
		0);
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (checkbutton2),
		1);

	checkbutton3 = gtk_check_button_new_with_mnemonic (
			_("Validate feed"));

	gtk_widget_show (checkbutton3);
	gtk_box_pack_start (
		GTK_BOX (vbox1),
		checkbutton3,
		FALSE,
		TRUE,
		0);
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (checkbutton3),
		1);

	gtk_file_chooser_set_extra_widget(data, vbox1);
	feed_html = 0;
	feed_validate = feed_enabled = 1;

	g_signal_connect(
		checkbutton1,
		"toggled",
		G_CALLBACK(import_toggle_cb_html),
		NULL);
	g_signal_connect(
		checkbutton2,
		"toggled",
		G_CALLBACK(import_toggle_cb_ena),
		NULL);
	g_signal_connect(
		checkbutton3,
		"toggled",
		G_CALLBACK(import_toggle_cb_valid),
		NULL);
	g_signal_connect(
		data,
		"response",
		G_CALLBACK(select_file_response),
		NULL);
	g_signal_connect(
		data,
		"destroy",
		G_CALLBACK(gtk_widget_destroy),
		data);
}

GtkWidget* create_import_dialog (void);

GtkWidget*
create_import_dialog (void)
{
	GtkWidget *import_file_select;
	GtkWidget *dialog_vbox5;
	GtkWidget *dialog_action_area5;
	GtkWidget *button1;
	GtkWidget *button2;

	import_file_select =
		gtk_file_chooser_dialog_new (
			_("Select import file"),
			NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			NULL,
			NULL);
	gtk_window_set_keep_above(
		GTK_WINDOW(import_file_select),
		TRUE);
	gtk_window_set_modal (
		GTK_WINDOW (import_file_select),
		TRUE);
	gtk_window_set_destroy_with_parent (
		GTK_WINDOW (import_file_select),
		TRUE);
	gtk_window_set_type_hint (
		GTK_WINDOW (import_file_select),
		GDK_WINDOW_TYPE_HINT_DIALOG);

#if GTK_CHECK_VERSION (2,14,0)
	dialog_vbox5 = gtk_dialog_get_content_area(GTK_DIALOG (import_file_select));
#else
	dialog_vbox5 = GTK_DIALOG (import_file_select)->vbox;
#endif
	gtk_widget_show (dialog_vbox5);

#if GTK_CHECK_VERSION (2,14,0)
	dialog_action_area5 = gtk_dialog_get_action_area(
				GTK_DIALOG (import_file_select));
#else
	dialog_action_area5 = GTK_DIALOG (import_file_select)->action_area;
#endif

	gtk_widget_show (dialog_action_area5);
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (dialog_action_area5),
		GTK_BUTTONBOX_END);

	button1 = gtk_button_new_from_stock ("gtk-cancel");
	gtk_widget_show (button1);
	gtk_dialog_add_action_widget (
		GTK_DIALOG (import_file_select),
		button1,
		GTK_RESPONSE_CANCEL);
#if GTK_CHECK_VERSION (2,18,0)
	gtk_widget_set_can_default (
		button1,
		TRUE);
#else
	GTK_WIDGET_SET_FLAGS (
		button1,
		GTK_CAN_DEFAULT);
#endif

	button2 = gtk_button_new_from_stock ("gtk-open");
	gtk_widget_show (button2);
	gtk_dialog_add_action_widget (
		GTK_DIALOG (import_file_select),
		button2,
		GTK_RESPONSE_OK);
#if GTK_CHECK_VERSION (2,18,0)
	gtk_widget_set_can_default(button2, TRUE);
#else
	GTK_WIDGET_SET_FLAGS (
		button2,
		GTK_CAN_DEFAULT);
#endif

	gtk_widget_grab_default (button2);
	return import_file_select;
}

GtkWidget* create_export_dialog (void);

GtkWidget*
create_export_dialog (void)
{
	GtkWidget *export_file_select;
	GtkWidget *vbox26;
	GtkWidget *hbuttonbox1;
	GtkWidget *button3;
	GtkWidget *button4;

	export_file_select = gtk_file_chooser_dialog_new (
				_("Select file to export"),
				NULL,
				GTK_FILE_CHOOSER_ACTION_SAVE,
				NULL,
				NULL);
	gtk_window_set_keep_above(
		GTK_WINDOW(export_file_select),
		TRUE);
	g_object_set (
		export_file_select,
		"local-only", FALSE,
		NULL);
	gtk_window_set_modal (
		GTK_WINDOW (export_file_select),
		TRUE);
	gtk_window_set_resizable (
		GTK_WINDOW (export_file_select),
		TRUE);
	gtk_window_set_destroy_with_parent (
		GTK_WINDOW (export_file_select),
		TRUE);
	gtk_window_set_type_hint (
		GTK_WINDOW (export_file_select),
		GDK_WINDOW_TYPE_HINT_DIALOG);

#if GTK_CHECK_VERSION (2,14,0)
	vbox26 = gtk_dialog_get_content_area(GTK_DIALOG (export_file_select));
#else
	vbox26 = GTK_DIALOG (export_file_select)->vbox;
#endif
	gtk_widget_show (vbox26);

#if GTK_CHECK_VERSION (2,14,0)
	hbuttonbox1 = gtk_dialog_get_action_area(
			GTK_DIALOG (export_file_select));
#else
	hbuttonbox1 = GTK_DIALOG (export_file_select)->action_area;
#endif
	gtk_widget_show (hbuttonbox1);
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (hbuttonbox1),
		GTK_BUTTONBOX_END);

	button3 = gtk_button_new_from_stock ("gtk-cancel");
	gtk_widget_show (button3);
	gtk_dialog_add_action_widget (
		GTK_DIALOG (export_file_select),
		button3,
		GTK_RESPONSE_CANCEL);
#if GTK_CHECK_VERSION (2,18,0)
	gtk_widget_set_can_default(button3, TRUE);
#else
	GTK_WIDGET_SET_FLAGS (
		button3,
		GTK_CAN_DEFAULT);
#endif
	button4 = gtk_button_new_from_stock ("gtk-save");
	gtk_widget_show (button4);
	gtk_dialog_add_action_widget (
		GTK_DIALOG (export_file_select),
		button4, GTK_RESPONSE_OK);
#if GTK_CHECK_VERSION (2,18,0)
	gtk_widget_set_can_default(button4, TRUE);
#else
	GTK_WIDGET_SET_FLAGS (
		button4,
		GTK_CAN_DEFAULT);
#endif

	gtk_widget_grab_default (button4);
	return export_file_select;
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

GList*
gen_folder_parents(
	GList *list, GList *flist, gchar *tmp);

/* for each feed folders generate complete list
 * of parents as directory list
 */

GList*
gen_folder_parents(GList *list, GList *flist, gchar *tmp)
{
	gchar **path = NULL;
	gchar *str = NULL;
	gint i;

	flist = g_list_first(flist);
	while ((flist = g_list_next(flist))) {
		if (!strncmp(tmp, flist->data, strlen(tmp))) {
			path = g_strsplit(flist->data, G_DIR_SEPARATOR_S, 0);
			i=0;
			str=NULL;
			if (*path != NULL) {
				do {
					if (!str)
						str = g_strdup(path[i]);
					else
						str = g_build_filename(
							str,
							path[i], NULL);
					if (!g_list_find_custom(list, str, (GCompareFunc)strcmp))
						list = g_list_append(list, str);
				} while (NULL != path[++i]);
				g_strfreev(path);
			}
			tmp = flist->data;
		}
	}
	return list;
}

void
gen_folder_list(
	gpointer key, gpointer value, gpointer user);

void
gen_folder_list(gpointer key, gpointer value, gpointer user)
{
	gchar *mf = get_main_folder();
	gchar *tmp = g_hash_table_lookup(
			rf->reversed_feed_folders, key);
	gchar *folder;
	d("mf:%s\n", mf);

	if (tmp) {
		d("tmp:%s\n", tmp);
		tmp = g_path_get_dirname(tmp);
		if (tmp && *tmp != '.')
			folder = g_build_path(G_DIR_SEPARATOR_S, mf, tmp, NULL);
		else
			folder = g_strdup(mf);
		g_free(tmp);
		if (!g_list_find_custom(flist, folder, (GCompareFunc)strcmp)) {
			d("append folder:%s\n", folder);
			flist = g_list_append(flist, folder);
		}
	}
	g_free(mf);
}

void
create_outline_feeds(
	gchar *key, gpointer value, gpointer user_data);

void
create_outline_feeds(
	gchar *key, gpointer value, gpointer user_data)
{
	gchar *dir = g_path_get_dirname(value);
	gchar *uid = lookup_key(key);
	gchar *tmp;
	if (!uid) goto out;
	if (!strcmp(user_data, dir)) {
		gchar *url = g_hash_table_lookup(rf->hr, uid);
		gchar *url_esc = g_markup_escape_text(url, strlen(url));
		gchar *key_esc = g_markup_escape_text(key, strlen(key));
		tmp = g_strdup_printf(
			"%s<outline title=\"%s\" text=\"%s\" description=\"%s\" type=\"rss\" xmlUrl=\"%s\" htmlUrl=\"%s\"/>\n",
			spacer->str, key_esc, key_esc, key_esc, url_esc, url_esc);
		strbuf = append_buffer(strbuf, tmp);
		g_free(key_esc);
		g_free(url_esc);
		g_free(tmp);
	}
out:	g_free(dir);
}

gchar *
append_buffer(gchar *result, gchar *str)
{
	if (result != NULL) {
		gchar *r = result;
		result = g_strconcat(result, str, NULL);
		g_free(r);
	} else {
		result = g_strdup(str);
	}
	return result;
}

gchar *
append_buffer_string(gchar *result, gchar *str);

gchar *
append_buffer_string(gchar *result, gchar *str)
{
	if (result != NULL) {
		gchar *r = result;
		result = g_strconcat(result, str, NULL);
		g_free(r);
	} else {
		result = g_strdup(str);
	}
	return result;
}

gchar *
create_folder_feeds(gchar *folder);

//generates all feeds's outline in folder
gchar *
create_folder_feeds(gchar *folder)
{
	gchar *tf;
	GList *names;
	gchar *mf = get_main_folder();
	GHashTable *nhash = g_hash_table_new(
				g_str_hash,
				g_str_equal);
	strbuf = NULL;
	if (folder && strcmp(folder, mf))
		tf = extract_main_folder(folder);
	else {
		tf = g_strdup(".");
		//get list of "unfoldered" feeds - silly approach
		names = g_hash_table_get_keys(rf->hrname);
		while ((names = g_list_next(names))) {
			if (!g_hash_table_lookup(rf->reversed_feed_folders, names->data))
				g_hash_table_insert(nhash,
					names->data, (gchar *)".");
		}
		g_hash_table_foreach(
			nhash,
			(GHFunc)create_outline_feeds, tf);
		g_list_free(names);
		g_hash_table_destroy(nhash);
	}
	g_hash_table_foreach(
		rf->reversed_feed_folders,
		(GHFunc)create_outline_feeds, tf);
	g_free(tf);
	g_free(mf);
	return strbuf;
}

gchar *
create_xml(GtkWidget *progress);

gchar *
create_xml(GtkWidget *progress)
{
	gchar *tmp, *result = NULL;
	GList *list = NULL;
	GList *p = NULL;
	GQueue *acc = g_queue_new();
	gint i = 0;
	gfloat fr;
	gchar *what;

	g_hash_table_foreach(
		rf->hrname,
		gen_folder_list,
		NULL);

	if (flist) {
		list = flist;
		tmp = list->data;
		//generate mssing parents
		while ((list = g_list_next(list))) {
			p = gen_folder_parents(p, list, tmp);
			tmp = list->data;
		}
		list = flist;
		//get parents into main list
		for (p = g_list_first(p); p != NULL; p = g_list_next(p)) {
			if (!g_list_find_custom(list, p->data, (GCompareFunc)g_ascii_strcasecmp)) {
				list = g_list_append(list, p->data);
			}
		}
		list = g_list_sort(list, (GCompareFunc)g_ascii_strcasecmp);
	} else {
		gchar *mf = get_main_folder();
		list = g_list_append(list, mf);
		g_free(mf);
	}
	spacer = g_string_new(NULL);


	tmp = list->data;
	strbuf = create_folder_feeds(tmp);
	result = append_buffer(result, strbuf);
	g_free(strbuf);
	while ((list = g_list_next(list))) {
top:		if (!tmp) break;
		if (!g_ascii_strncasecmp(tmp, list->data, strlen(tmp))) {
			gchar *tname, *ttmp;
			g_queue_push_tail(acc, tmp);
			ttmp = g_strconcat(tmp, "/", NULL);
			d("cutter:%s\n", ttmp);
			d("data:%s\n", (gchar *)list->data);
			tname = strextr((gchar *)list->data, ttmp);
			strbuf = g_strdup_printf(
					"%s<outline title=\"%s\" text=\"%s\" description=\"%s\" type=\"folder\">\n",
					spacer->str, tname, tname, tname);
			g_free(tname);
			g_free(ttmp);
			g_string_append(spacer, "    ");
			result = append_buffer(result, strbuf);
			g_free(strbuf);
			strbuf = create_folder_feeds(list->data);
			result = append_buffer(result, strbuf);
			g_free(strbuf);
		} else {
			gchar *tname;
			g_string_truncate(spacer, strlen(spacer->str)-4);
			tname = g_strdup_printf("%s</outline>\n", spacer->str);
			result = append_buffer_string(
					result, tname);
			g_free(tname);
			tmp = g_queue_pop_tail(acc);
			goto top;
		}
		tmp = list->data;

	count++;
	fr = ((count*100)/g_hash_table_size(rf->hr));
	gtk_progress_bar_set_fraction((GtkProgressBar *)progress, fr/100);
	what = g_strdup_printf(_("%2.0f%% done"), fr);
	gtk_progress_bar_set_text((GtkProgressBar *)progress, what);
	g_free(what);

	}
	for (i=1;i<=g_queue_get_length(acc);i++) {
		gchar *tname;
		g_string_truncate(spacer, strlen(spacer->str)-4);
		tname = g_strdup_printf("%s</outline>\n", spacer->str);
		result = append_buffer_string(
				result, tname);
		g_free(tname);
	}
	g_string_free(spacer, TRUE);
	return result;
}

void export_opml(gchar *file);

void
export_opml(gchar *file)
{
	GtkWidget *import_dialog;
	GtkWidget *import_label;
	GtkWidget *import_progress;
	GtkWidget *content_area;
	char outstr[200];
	gchar *opml;
	time_t t;
	struct tm *tmp;
	FILE *f;

	gchar *msg = g_strdup(_("Exporting feeds..."));
#if EVOLUTION_VERSION < 22904
	import_dialog = e_error_new(
			GTK_WINDOW(rf->preferences),
			"shell:importing",
			msg,
			NULL);

#else
	import_dialog = e_alert_dialog_new_for_args(
			GTK_WINDOW(rf->preferences),
			"shell:importing",
			msg,
			NULL);
#endif
	gtk_window_set_keep_above(GTK_WINDOW(import_dialog), TRUE);
//        g_signal_connect(import_dialog, "response", G_CALLBACK(import_dialog_response), NULL);
	import_label = gtk_label_new(_("Please wait"));
	import_progress = gtk_progress_bar_new();
#if GTK_CHECK_VERSION (2,14,0)
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(import_dialog));
#else
	content_area = GTK_DIALOG(import_dialog)->vbox;
#endif
	gtk_box_pack_start(
		GTK_BOX(content_area),
		import_label,
		FALSE,
		FALSE,
		0);
	gtk_box_pack_start(
		GTK_BOX(content_area),
		import_progress,
		FALSE,
		FALSE,
		0);
	gtk_widget_show_all(import_dialog);
	g_free(msg);
	count = 0;
	strbuf = create_xml(import_progress);
	gtk_widget_destroy(import_dialog);
	t = time(NULL);
	tmp = localtime(&t);
	strftime(
		outstr,
		sizeof(outstr),
		"%a, %d %b %Y %H:%M:%S %z",
		tmp);
	opml = g_strdup_printf("<opml version=\"1.1\">\n<head>\n"
		"<title>Evolution-RSS Exported Feeds</title>\n"
		"<dateModified>%s</dateModified>\n</head>\n<body>%s</body>\n</opml>\n",
		outstr,
		strbuf);
	g_free(strbuf);

	f = fopen(file, "w+");
	if (f) {
		fwrite(opml, strlen(opml), 1, f);
		fclose(f);
	} else {
#if EVOLUTION_VERSION < 22904
		e_error_run(
			GTK_WINDOW(rf->preferences),
			"org-gnome-evolution-rss:feederr",
			_("Error exporting feeds!"),
			g_strerror(errno),
			NULL);

#else
		e_alert_run_dialog_for_args(
			GTK_WINDOW(rf->preferences),
			"org-gnome-evolution-rss:feederr",
			_("Error exporting feeds!"),
			g_strerror(errno),
			NULL);
#endif
	}
	g_free(opml);

}


static void
select_export_response(
	GtkWidget *selector, guint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_OK) {
		char *name;

		name = gtk_file_chooser_get_filename (
			GTK_FILE_CHOOSER (selector));
		if (name) {
			gtk_widget_destroy(selector);
			export_opml(name);
			g_free(name);
		}
	} else
		gtk_widget_destroy(selector);

}

/*
 * unfortunately versions earlier than libsoup-2.26 cannot
 * manipulate cookies so we will disable import cookies function completely
 */

#if LIBSOUP_VERSION >= 2026000
SoupCookieJar *
import_cookies(gchar *file)
{
	FILE *f;
	SoupCookieJar *jar = NULL;
	gchar header[16];

	memset(header, 0, 16);
	d("import cookies from %s\n", file);
	f = fopen(file, "r");
	if (f) {
		fgets(header, 16, f);
		fclose(f);
		if (!g_ascii_strncasecmp(header, SQLITE_MAGIC, sizeof(SQLITE_MAGIC))) {
#ifdef HAVE_LIBSOUP_GNOME
			jar = soup_cookie_jar_db_new(file, TRUE);
#else
			g_print("Importing sqlite format requires libsoup-gnome\n");
#endif
		} else
			jar = soup_cookie_jar_text_new(file, TRUE);
	}
	return jar;
}

void inject_cookie(SoupCookie *cookie, GtkProgressBar *progress);

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
	}
}

void
process_cookies(SoupCookieJar *jar)
{
	GSList *list = NULL;
	gchar *msg = g_strdup(_("Importing cookies..."));
	GtkWidget *import_dialog, *import_label, *import_progress;
	GtkWidget *content_area;

	ccurrent = 0;
	ctotal = 0;
	list = soup_cookie_jar_all_cookies(jar);
#if EVOLUTION_VERSION < 22904
	import_dialog = e_error_new(
#else
	import_dialog = e_alert_dialog_new_for_args(
#endif
			GTK_WINDOW(rf->preferences),
			"shell:importing",
			msg,
			NULL);
	gtk_window_set_keep_above(
		GTK_WINDOW(import_dialog),
		TRUE);
	g_signal_connect(
		import_dialog,
		"response",
		G_CALLBACK(import_dialog_response),
		NULL);
	import_label = gtk_label_new(_("Please wait"));
	import_progress = gtk_progress_bar_new();
	content_area = gtk_dialog_get_content_area(GTK_DIALOG(import_dialog));
	gtk_box_pack_start(
		GTK_BOX(content_area),
		import_label,
		FALSE,
		FALSE,
		0);
	gtk_box_pack_start(
		GTK_BOX(content_area),
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
select_import_cookies_response(GtkWidget *selector, guint response, gpointer user_data)
{
	SoupCookieJar *jar;

	if (response == GTK_RESPONSE_OK) {
		char *name;

		name = gtk_file_chooser_get_filename (
			GTK_FILE_CHOOSER (selector));
		if (name) {
			gtk_widget_destroy(selector);
			if ((jar = import_cookies(name)))
				process_cookies(jar);
			g_free(name);
		}
	} else
		gtk_widget_destroy(selector);

}

GtkWidget*
create_import_cookies_dialog (void)
{
	GtkWidget *import_file_select;
	GtkWidget *vbox26;
	GtkWidget *hbuttonbox1;
	GtkWidget *button3;
	GtkWidget *button4;

	import_file_select = gtk_file_chooser_dialog_new (
				_("Select file to import"),
				NULL,
				GTK_FILE_CHOOSER_ACTION_SAVE,
				NULL,
				NULL);
	gtk_window_set_keep_above(
		GTK_WINDOW(import_file_select),
		TRUE);
	g_object_set (
		import_file_select,
		"local-only",
		FALSE,
		NULL);
	gtk_window_set_modal (
		GTK_WINDOW (import_file_select),
		TRUE);
	gtk_window_set_resizable (
		GTK_WINDOW (import_file_select),
		TRUE);
	gtk_window_set_destroy_with_parent (
		GTK_WINDOW (import_file_select),
		TRUE);
	gtk_window_set_type_hint (
		GTK_WINDOW (import_file_select),
		GDK_WINDOW_TYPE_HINT_DIALOG);

	vbox26 = gtk_dialog_get_content_area(GTK_DIALOG (import_file_select));
	gtk_widget_show (vbox26);

	hbuttonbox1 = GTK_WIDGET(gtk_dialog_get_action_area(GTK_DIALOG(import_file_select)));
	gtk_widget_show (hbuttonbox1);
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (hbuttonbox1),
		GTK_BUTTONBOX_END);

	button3 = gtk_button_new_from_stock ("gtk-cancel");
	gtk_widget_show (button3);
	gtk_dialog_add_action_widget (
		GTK_DIALOG (import_file_select),
		button3,
		GTK_RESPONSE_CANCEL);
	gtk_widget_set_can_default(button3, TRUE);

	button4 = gtk_button_new_from_stock ("gtk-save");
	gtk_widget_show (button4);
	gtk_dialog_add_action_widget (
		GTK_DIALOG (import_file_select),
		button4,
		GTK_RESPONSE_OK);
	gtk_widget_set_can_default(button4, TRUE);

	gtk_widget_grab_default (button4);
	return import_file_select;
}

void
decorate_import_cookies_fs (gpointer data)
{
	GtkFileFilter *file_filter = gtk_file_filter_new ();
	GtkFileFilter *filter = gtk_file_filter_new ();
	gtk_dialog_set_default_response (GTK_DIALOG (data), GTK_RESPONSE_OK);
	gtk_file_chooser_set_local_only (data, FALSE);

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

	gtk_file_filter_add_pattern (filter, "*.txt");
	gtk_file_filter_add_pattern (filter, "*.sqlite");
	gtk_file_chooser_set_filter(data, filter);
	g_signal_connect(
		data,
		"response",
		G_CALLBACK(select_import_cookies_response),
		data);
	g_signal_connect(
		data,
		"destroy",
		G_CALLBACK(gtk_widget_destroy),
		data);
}

static void
import_cookies_cb (GtkWidget *widget, gpointer data)
{
	GtkWidget *import = create_import_cookies_dialog();
	decorate_import_cookies_fs(import);
	gtk_widget_show(import);
}
#endif

static void
decorate_export_fs (gpointer data)
{
	GtkFileFilter *file_filter = gtk_file_filter_new ();
	GtkFileFilter *filter = gtk_file_filter_new ();

	gtk_file_chooser_set_do_overwrite_confirmation (
		GTK_FILE_CHOOSER (data),
		TRUE);
	gtk_dialog_set_default_response (
		GTK_DIALOG (data),
		GTK_RESPONSE_OK);
	gtk_file_chooser_set_local_only (data, FALSE);
	gtk_file_chooser_set_current_name (data, "evolution-rss.opml");

	gtk_file_filter_add_pattern (
		GTK_FILE_FILTER(file_filter),
		"*");
	gtk_file_filter_set_name (
		GTK_FILE_FILTER(file_filter),
		_("All Files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (data),
		GTK_FILE_FILTER(file_filter));

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (
		GTK_FILE_FILTER(file_filter),
		"*.xml");
	gtk_file_filter_set_name (
		GTK_FILE_FILTER(file_filter),
		_("XML Files"));
	gtk_file_chooser_add_filter (
		GTK_FILE_CHOOSER (data),
		GTK_FILE_FILTER(file_filter));

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (
		GTK_FILE_FILTER(file_filter),
		"*.opml");
	gtk_file_filter_set_name (
		GTK_FILE_FILTER(file_filter),
		_("OPML Files"));
	gtk_file_chooser_add_filter (
		GTK_FILE_CHOOSER (data),
		GTK_FILE_FILTER(file_filter));


	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (data),
		GTK_FILE_FILTER(file_filter));


	gtk_file_filter_add_pattern (filter, "*.opml");
	gtk_file_filter_add_pattern (filter, "*.xml");
	gtk_file_chooser_set_filter(data, filter);
	g_signal_connect(
		data,
		"response",
		G_CALLBACK(select_export_response),
		data);
	g_signal_connect(
		data,
		"destroy",
		G_CALLBACK(gtk_widget_destroy),
		data);
}

static void
export_cb (GtkWidget *widget, gpointer data)
{
	if (!rf->import) {
		GtkWidget *export = create_export_dialog();
		decorate_export_fs(export);
		gtk_dialog_set_default_response (
			GTK_DIALOG (export),
			GTK_RESPONSE_OK);
		if (g_hash_table_size(rf->hrname)<1) {
#if EVOLUTION_VERSION < 22904
			e_error_run(GTK_WINDOW(export),
				"org-gnome-evolution-rss:generr",
				_("No RSS feeds configured!\nUnable to export."),
				NULL);
#else
			e_alert_run_dialog_for_args(GTK_WINDOW(export),
				"org-gnome-evolution-rss:generr",
				_("No RSS feeds configured!\nUnable to export."),
				NULL);
#endif
			return;
		}
		gtk_widget_show(export);

//              g_signal_connect(data, "response", G_CALLBACK(select_export_response), data);
//              g_signal_connect(data, "destroy", G_CALLBACK(gtk_widget_destroy), data);
	}
	return;
}


static void
spin_update_cb (GtkWidget *widget, gchar *key)
{
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default();
	gconf_client_set_float (
		client,
		key,
		gtk_spin_button_get_value((GtkSpinButton*)widget),
		NULL);
	g_object_unref(client);
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
	g_settings_set_double (settings, key,
		gtk_spin_button_get_value((GtkSpinButton*)widget));
	g_object_unref(settings);
#endif
}

static void
destroy_ui_data (gpointer data)
{
	UIData *ui = (UIData *) data;

	g_return_if_fail(ui != NULL);

	g_object_unref (ui->xml);
	g_free (ui);
}

void
font_cb(GtkWidget *widget, GtkWidget *data)
{
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default();
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
#endif
	gboolean active = 1-gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (widget));
	/* Save the new setting to gconf */
#if EVOLUTION_VERSION < 30304
	gconf_client_set_bool (client,
		GCONF_KEY_CUSTOM_FONT, active, NULL);
#else
	g_settings_set_boolean (settings,
		CONF_CUSTOM_FONT, active);
#endif
	gtk_widget_set_sensitive(data, active);
#if EVOLUTION_VERSION < 30304
	g_object_unref(client);
#else
	g_object_unref(settings);
#endif
}

GtkWidget *e_plugin_lib_get_configure_widget (EPlugin *epl);

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	GtkListStore  *store;
	GtkTreeIter iter;
	GtkWidget *hbox, *combo;
#if !defined(HAVE_GECKO) && !defined (HAVE_WEBKIT)
	GtkWidget *label_webkit;
#endif
	GtkCellRenderer *renderer;
	guint i, render;
	UIData *ui = g_new0 (UIData, 1);
	char *uifile;
	gdouble adj;
	GError* error = NULL;
	gchar *toplevel[] = {(gchar *)"settingsbox", NULL};
	GtkAdjustment *adjustment;
	GtkWidget *widget1, *widget2;
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default ();
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
#endif

	uifile = g_build_filename (EVOLUTION_UIDIR,
		"rss-html-rendering.ui",
		NULL);
	ui->xml = gtk_builder_new ();
#if GTK_CHECK_VERSION (2,14,0)
	if (!gtk_builder_add_objects_from_file (ui->xml, uifile, toplevel, &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}
#else
	g_warning("gtk too old! cannot create ui file. need >= 2.14\n");
	g_free(ui);
	// and not very interesed to back port this
	return NULL;
#endif
	g_free (uifile);

	ui->combobox = GTK_WIDGET (gtk_builder_get_object(ui->xml, "hbox1"));
	renderer = gtk_cell_renderer_text_new ();
	store = gtk_list_store_new(1, G_TYPE_STRING);
	combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	for (i=0;i<3;i++) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, _(engines[i].label), -1);
	}
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo),
		renderer,
		"text", 0,
		NULL);
#if EVOLUTION_VERSION < 30304
	render = GPOINTER_TO_INT(gconf_client_get_int(client,
		GCONF_KEY_HTML_RENDER,
		NULL));
#else
	render = g_settings_get_int(settings, CONF_HTML_RENDER);
#endif

	switch (render) {
		case 10:
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
			break;
		case 1:
#ifdef HAVE_WEBKIT
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 1);
#endif
			break;
		case 2:
#ifndef HAVE_GECKO
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 2);
#endif
			break;
		default:
			g_print("Selected render not supported! Failling back to default.\n");
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), render);
	}

	gtk_cell_layout_set_cell_data_func (
		GTK_CELL_LAYOUT (combo),
		renderer,
		set_sensitive,
		NULL, NULL);

#if !defined(HAVE_GECKO) && !defined (HAVE_WEBKIT)
	label_webkit = GTK_WIDGET (
			gtk_builder_get_object(ui->xml, "label_webkits"));
	gtk_label_set_text(GTK_LABEL(label_webkit),
		_("Note: In order to be able to use Mozilla (Firefox) or Apple Webkit \nas renders you need firefox or webkit devel package \ninstalled and evolution-rss should be recompiled to see those packages."));
	gtk_widget_show(label_webkit);
#endif
	g_signal_connect (
		combo,
		"changed",
		G_CALLBACK (render_engine_changed),
		NULL);
	gtk_widget_show(combo);
	gtk_box_pack_start(
		GTK_BOX(ui->combobox),
		combo,
		FALSE,
		FALSE,
		0);

	widget1 = GTK_WIDGET (
			gtk_builder_get_object(
				ui->xml, "fontsize"));
	widget2 = GTK_WIDGET (
			gtk_builder_get_object(
				ui->xml, "fontsetting"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget2),
#if EVOLUTION_VERSION < 30304
		1 - gconf_client_get_bool (
			client,
			GCONF_KEY_CUSTOM_FONT, NULL));
#else
		1 - g_settings_get_boolean (settings, CONF_CUSTOM_FONT));
#endif
	g_object_set(widget1, "sensitive", (gboolean)1-gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (widget2)), NULL);
	g_signal_connect (
		widget2,
		"toggled",
		G_CALLBACK (font_cb),
		widget1);

	ui->minfont = GTK_WIDGET (
			gtk_builder_get_object(
				ui->xml, "minfont"));
	/*setup the adjustment*/
	adjustment = (GtkAdjustment *)gtk_adjustment_new(
			12,	//DEFAULT MIN FONT
			1,	//DEFAULT MIN FONT
			100,
			1,
			1,
			0);
	gtk_spin_button_set_adjustment(
		(GtkSpinButton *)ui->minfont,
		adjustment);
#if EVOLUTION_VERSION < 30304
	adj = gconf_client_get_float(
			client,
			GCONF_KEY_MIN_FONT_SIZE,
			NULL);
#else
	adj = g_settings_get_double(settings, CONF_MIN_FONT_SIZE);
#endif
	if (adj)
		gtk_spin_button_set_value(
			(GtkSpinButton *)ui->minfont, adj);
	g_signal_connect(
		ui->minfont,
		"changed",
		G_CALLBACK(spin_update_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_MIN_FONT_SIZE);
#else
		(gpointer)CONF_MIN_FONT_SIZE);
#endif
	g_signal_connect(
		ui->minfont,
		"value-changed",
		G_CALLBACK(spin_update_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_MIN_FONT_SIZE);
#else
		(gpointer)CONF_MIN_FONT_SIZE);
#endif

	ui->check = GTK_WIDGET (
		gtk_builder_get_object(ui->xml, "enable_java"));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (ui->check),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(
			client, GCONF_KEY_HTML_JAVA, NULL));
#else
		g_settings_get_boolean(settings, CONF_HTML_JAVA));
#endif
	g_signal_connect(ui->check,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_HTML_JAVA);
#else
		(gpointer)CONF_HTML_JAVA);
#endif

	ui->check = GTK_WIDGET (
			gtk_builder_get_object(ui->xml, "image_resize"));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (ui->check),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(
			client, GCONF_KEY_IMAGE_RESIZE, NULL));
#else
		g_settings_get_boolean(settings, CONF_IMAGE_RESIZE));
#endif
	g_signal_connect(ui->check,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_IMAGE_RESIZE);
#else
		(gpointer)CONF_IMAGE_RESIZE);
#endif

	ui->check = GTK_WIDGET (
			gtk_builder_get_object(ui->xml, "enable_js"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->check),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(client, GCONF_KEY_HTML_JS, NULL));
#else
		g_settings_get_boolean(settings, CONF_HTML_JS));
#endif
	g_signal_connect(ui->check,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_HTML_JS);
#else
		(gpointer)CONF_HTML_JS);
#endif

	ui->check = GTK_WIDGET (
			gtk_builder_get_object(ui->xml, "accept_cookies"));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (ui->check),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(
			client, GCONF_KEY_ACCEPT_COOKIES, NULL));
#else
		g_settings_get_boolean(
			settings, CONF_ACCEPT_COOKIES));
#endif
	g_signal_connect(ui->check,
		"clicked",
		G_CALLBACK(accept_cookies_cb),
		ui->import);
	ui->import = GTK_WIDGET (
			gtk_builder_get_object(ui->xml, "import_cookies"));
	//we have to have ui->import looked up

#if LIBSOUP_VERSION >= 2026000
	g_signal_connect(
		ui->import,
		"clicked",
		G_CALLBACK(import_cookies_cb),
		ui->import);
#else
	gtk_widget_set_sensitive(ui->import, FALSE);
	gtk_widget_set_sensitive(ui->check, FALSE);
#endif

	ui->nettimeout = GTK_WIDGET (
				gtk_builder_get_object(
					ui->xml, "nettimeout"));
	/* setup the adjustment*/
	adjustment = (GtkAdjustment *)gtk_adjustment_new(
			NETWORK_MIN_TIMEOUT,
			NETWORK_MIN_TIMEOUT,
			3600,
			1,
			1,
			0);
	gtk_spin_button_set_adjustment(
		(GtkSpinButton *)ui->nettimeout,
		adjustment);
#if EVOLUTION_VERSION < 30304
	adj = gconf_client_get_float(
			client,
			GCONF_KEY_NETWORK_TIMEOUT,
			NULL);
#else
	adj = g_settings_get_double(settings, CONF_NETWORK_TIMEOUT);
#endif
	if (adj < NETWORK_MIN_TIMEOUT) {
		adj = NETWORK_MIN_TIMEOUT;
#if EVOLUTION_VERSION < 30304
		gconf_client_set_float (
			client,
			GCONF_KEY_NETWORK_TIMEOUT,
			adj,
			NULL);
#else
		g_settings_set_double (
			settings, CONF_NETWORK_TIMEOUT, adj);
#endif
	}
	if (adj)
		gtk_spin_button_set_value(
			(GtkSpinButton *)ui->nettimeout, adj);
	g_signal_connect(
		ui->nettimeout,
		"changed",
		G_CALLBACK(spin_update_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_NETWORK_TIMEOUT);
#else
		(gpointer)CONF_NETWORK_TIMEOUT);
#endif
	g_signal_connect(
		ui->nettimeout,
		"value-changed",
		G_CALLBACK(spin_update_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_NETWORK_TIMEOUT);
#else
		(gpointer)CONF_NETWORK_TIMEOUT);
#endif

	//feed notification
	ui->check = GTK_WIDGET (
			gtk_builder_get_object(ui->xml, "status_icon"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->check),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(
			client, GCONF_KEY_STATUS_ICON, NULL));
#else
		g_settings_get_boolean(settings, CONF_STATUS_ICON));
#endif
	g_signal_connect(ui->check,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_STATUS_ICON);
#else
		(gpointer)CONF_STATUS_ICON);
#endif
	ui->check = GTK_WIDGET (
			gtk_builder_get_object(ui->xml, "blink_icon"));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (ui->check),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(
			client, GCONF_KEY_BLINK_ICON, NULL));
#else
		g_settings_get_boolean(
			settings, CONF_BLINK_ICON));
#endif
	g_signal_connect(ui->check,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_BLINK_ICON);
#else
		(gpointer)CONF_BLINK_ICON);
#endif
	ui->check = GTK_WIDGET (
			gtk_builder_get_object(ui->xml, "feed_icon"));
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (ui->check),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(client, GCONF_KEY_FEED_ICON, NULL));
#else
		g_settings_get_boolean(settings, CONF_FEED_ICON));
#endif
	g_signal_connect(ui->check,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_FEED_ICON);
#else
		(gpointer)CONF_FEED_ICON);
#endif

#if GTK_MAJOR_VERSION < 3
	hbox = gtk_vbox_new (FALSE, 0);
#else
	hbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
#endif

	gtk_box_pack_start (
		GTK_BOX (hbox),
		GTK_WIDGET (gtk_builder_get_object(ui->xml, "settingsbox")),
		FALSE,
		FALSE,
		0);

	g_object_set_data_full (
		G_OBJECT (hbox),
		"ui-data",
		ui,
		destroy_ui_data);
#if EVOLUTION_VERSION < 30304
	g_object_unref(client);
#else
	g_object_unref(settings);
#endif

	return hbox;
}

typedef struct _EConfigTargetRSS EConfigTargetRSS;
struct _EConfigTargetRSS
{
	gchar *label;
} ER;

void rss_folder_factory_abort (EPlugin *epl, EConfigTarget *target);

void rss_folder_factory_abort (EPlugin *epl, EConfigTarget *target)
{
	d("abort");
}

void rss_folder_factory_commit (EPlugin *epl, EConfigTarget *target);

void rss_folder_factory_commit (EPlugin *epl, EConfigTarget *target)
{
	const gchar *user = NULL, *pass = NULL;
	GtkWidget *entry1, *checkbutton1, *checkbutton2;
	GtkWidget *checkbutton3, *checkbutton4;
	GtkWidget *radiobutton1, *radiobutton2, *radiobutton3;
	GtkWidget *radiobutton4, *radiobutton5, *radiobutton6;
	GtkWidget *radiobutton7;
	GtkWidget *spinbutton1, *spinbutton2;
	GtkWidget *ttl_value, *feed_name_entry;
	GtkWidget *authuser, *authpass, *useauth;
	gchar *feed_name;
	gboolean fhtml, auth_enabled;
	guint i=0;
	gchar *key = NULL;

	add_feed *feed = (add_feed *)g_object_get_data((GObject *)target->config->widget, "add-feed");
	gchar *url = (gchar *)g_object_get_data((GObject *)target->config->widget, "url");
	gchar *ofolder = (gchar *)g_object_get_data((GObject *)target->config->widget, "ofolder");

	EMConfigTargetFolder *targetfolder =
		(EMConfigTargetFolder *)target->config->target;
	gchar *main_folder = lookup_main_folder();
#if (DATASERVER_VERSION >= 2031001)
	gchar *folder = (gchar *)camel_folder_get_full_name(targetfolder->folder);
#else
	gchar *folder = targetfolder->folder->full_name;
#endif

	if (folder == NULL
			|| g_ascii_strncasecmp(folder, main_folder, strlen(main_folder))
			|| !g_ascii_strcasecmp(folder, main_folder))
		return;

	key = lookup_key(ofolder);
	if (!key) return;

	gtk_widget_set_sensitive(target->config->widget, FALSE);

	entry1 = GTK_WIDGET (gtk_builder_get_object(feed->gui, "url_entry"));
	checkbutton1 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "html_check"));
	checkbutton2 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "enabled_check"));
	checkbutton3 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "validate_check"));
	checkbutton4 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_unread"));
	radiobutton1 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_rb1"));
	radiobutton2 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_rb2"));
	radiobutton3 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_rb3"));
	radiobutton4 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "ttl_global"));
	radiobutton5 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "ttl"));
	radiobutton6 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "ttl_disabled"));
	radiobutton7 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_rb4"));
	spinbutton1 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_sb1"));
	spinbutton2 = GTK_WIDGET (gtk_builder_get_object (feed->gui, "storage_sb2"));
	ttl_value = (GtkWidget *)GTK_WIDGET (gtk_builder_get_object (feed->gui, "ttl_value"));
	feed_name_entry = (GtkWidget *)GTK_WIDGET (gtk_builder_get_object (feed->gui, "feed_name"));
	feed_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(feed_name_entry)));


	fhtml = feed->fetch_html;
	feed->feed_url = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry1)));
	fhtml = gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (checkbutton1));
	feed->fetch_html = fhtml;
	feed->enabled = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(checkbutton2));
	feed->validate = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(checkbutton3));
	while (i<4) {
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
	feed->del_notpresent = gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(radiobutton7));
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
	if (feed_name && 0 != g_ascii_strcasecmp(feed_name, ofolder))
		feed->renamed = 0;
	else
		feed->renamed = 1;

	authuser = GTK_WIDGET (gtk_builder_get_object(feed->gui, "auth_user"));
	authpass = GTK_WIDGET (gtk_builder_get_object(feed->gui, "auth_pass"));
	useauth = GTK_WIDGET (gtk_builder_get_object(feed->gui, "use_auth"));

	process_dialog_edit(feed, url, ofolder);

	user = gtk_entry_get_text(GTK_ENTRY(authuser));
	pass = gtk_entry_get_text(GTK_ENTRY(authpass));
	auth_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (useauth));

	if (user)
		g_hash_table_remove(rf->hruser, url);

	if (pass)
		g_hash_table_remove(rf->hrpass, url);

	if (auth_enabled) {
		g_hash_table_insert(rf->hruser, g_strdup(url),
			g_strdup(gtk_entry_get_text (GTK_ENTRY (authuser))));
		g_hash_table_insert(rf->hrpass, g_strdup(url),
			g_strdup(gtk_entry_get_text (GTK_ENTRY (authpass))));
		save_up(url);
	} else
		del_up(url);
}

GtkWidget *rss_folder_factory (EPlugin *epl, EConfigHookItemFactoryData *data);

GtkWidget *
rss_folder_factory (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetFolder *target = (EMConfigTargetFolder *)data->config->target;
	gchar *url = NULL, *ofolder = NULL;
	gchar *main_folder = lookup_main_folder();
#if (DATASERVER_VERSION >= 2031001)
	gchar *folder = (gchar *)camel_folder_get_full_name(target->folder);
#else
	gchar *folder = target->folder->full_name;
#endif
	add_feed *feed = NULL;
	GtkWidget *action_area;
	gpointer key;
	gboolean found;

	//filter only rss folders
	if (folder == NULL
		|| g_ascii_strncasecmp(folder, main_folder, strlen(main_folder))
		|| !g_ascii_strcasecmp(folder, main_folder))
			goto out;

	ofolder = lookup_original_folder(folder, &found);
	key = lookup_key(ofolder);
	if (!key) {
		g_free(ofolder);
		goto out;
	}

	url = g_hash_table_lookup(rf->hr, key);
	if (url) {
		feed = build_dialog_add(url, ofolder);
		//we do not need ok/cancel buttons here

#if GTK_CHECK_VERSION (2,14,0)
		action_area = gtk_dialog_get_action_area(GTK_DIALOG(feed->dialog));
#else
		action_area = GTK_DIALOG (feed->dialog)->action_area;
#endif
		gtk_widget_hide(action_area);
		g_object_ref(feed->child);
		gtk_container_remove (
			GTK_CONTAINER (gtk_widget_get_parent(feed->child)),
			feed->child);
		gtk_notebook_remove_page(
			(GtkNotebook *) data->parent,
			0);
		gtk_notebook_insert_page(
			(GtkNotebook *) data->parent,
			(GtkWidget *) feed->child,
			NULL,
			0);
		g_object_set_data_full (
			G_OBJECT (data->parent),
			"add-feed",
			feed,
			NULL);
		g_object_set_data_full (G_OBJECT (data->parent), "url", url, NULL);
		g_object_set_data_full (G_OBJECT (data->parent), "ofolder", ofolder, NULL);
		return feed->child;
	}

out:	return NULL;
}

GtkWidget *
#if EVOLUTION_VERSION >= 23106
rss_config_control_new (EShell *shell);
#else
rss_config_control_new (void);
#endif

GtkWidget *
#if EVOLUTION_VERSION >= 23106
rss_config_control_new (EShell *shell)
#else
rss_config_control_new (void)
#endif
{
	GtkWidget *control_widget;
	GtkWidget *button1, *button2, *button3;
	gchar *uifile;
	GtkBuilder *gui;
	GtkWidget
		*check1,
		*check2,
		*check3,
		*check4,
		*check5,
		*check6,
		*check7;
	GtkWidget *spin;
	GtkWidget *enclsize;
	GtkWidget *import;
	GtkWidget *export;
	GtkListStore  *store;
	GtkTreeIter    iter;
	GtkCellRenderer *cell;
	GtkTreeView *treeview;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	gdouble adj, size;
	GError* error = NULL;
#if EVOLUTION_VERSION < 30304
	GConfClient *client = gconf_client_get_default();
#else
	GSettings *settings = g_settings_new(RSS_CONF_SCHEMA);
#endif

	d("rf->%p\n", rf);
	uifile = g_build_filename (
			EVOLUTION_UIDIR,
			"rss-main.ui",
			NULL);
	gui = gtk_builder_new ();
	if (!gtk_builder_add_from_file (gui, uifile, &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}
	g_free (uifile);

	treeview = (GtkTreeView *)gtk_builder_get_object(
					gui, "feeds-treeview");
	rf->treeview = (GtkWidget *)treeview;

	gtk_tree_view_set_rules_hint (
		treeview,
		TRUE);

	store = gtk_list_store_new (
			5,
			G_TYPE_BOOLEAN,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING);

	gtk_tree_view_set_model (
		treeview,
		(GtkTreeModel *)store);

	cell = gtk_cell_renderer_toggle_new ();

	column = gtk_tree_view_column_new_with_attributes (
			_("Enabled"),
			cell,
			"active",
			0,
			NULL);
	g_signal_connect(
		(gpointer) cell,
		"toggled",
		G_CALLBACK(enable_toggle_cb),
		store);
	gtk_tree_view_column_set_resizable(column, FALSE);
	gtk_tree_view_column_set_max_width (column, 70);
	gtk_tree_view_append_column (
		treeview,
		column);
	cell = gtk_cell_renderer_text_new ();
	g_object_set (
		cell,
		"ellipsize",
		PANGO_ELLIPSIZE_END,
		NULL);
	g_object_set (
		cell,
		"is-expanded",
		TRUE,
		NULL);
	column = gtk_tree_view_column_new_with_attributes (
			_("Feed Name"),
			cell,
			"text",
				1,
			NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_append_column (
		treeview,
		column);
	gtk_tree_view_column_set_sort_column_id (column, 1);
	gtk_tree_view_column_clicked(column);
	column = gtk_tree_view_column_new_with_attributes (
			_("Type"),
			cell,
			"text", 2,
			NULL);
//	gtk_tree_view_column_set_resizable(column, TRUE);
//	gtk_tree_view_column_set_expand(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 111);
//	gtk_tree_view_column_set_min_width (column, -1);
	gtk_tree_view_append_column (
		treeview,
		column);
	gtk_tree_view_column_set_sort_column_id (column, 2);
	gtk_tree_view_set_search_column (
		treeview,
		2);
	gtk_tree_view_set_search_column(treeview, 1);
#if GTK_CHECK_VERSION (2,12,0)
	gtk_tree_view_set_tooltip_column (treeview, 3);
#endif

	if (rf->hr != NULL)
		g_hash_table_foreach(rf->hrname, construct_list, store);

	//make sure something (first row) is selected
	selection = gtk_tree_view_get_selection(treeview);
	if (gtk_tree_model_iter_nth_child(
		GTK_TREE_MODEL(store),
		&iter,
		NULL,
		0))
		gtk_tree_selection_select_iter(selection, &iter);

	gtk_tree_view_columns_autosize (treeview);
	g_signal_connect (treeview,
		"row_activated",
		G_CALLBACK(treeview_row_activated),
		treeview);

	button1 = GTK_WIDGET (gtk_builder_get_object(
			gui,
			"feed-add-button"));
	g_signal_connect(
		button1,
		"clicked",
		G_CALLBACK(feeds_dialog_add),
		treeview);

	button2 = GTK_WIDGET (gtk_builder_get_object(
			gui,
			"feed-edit-button"));
	g_signal_connect(
		button2,
		"clicked",
		G_CALLBACK(feeds_dialog_edit),
		treeview);

	button3 = GTK_WIDGET (gtk_builder_get_object(
			gui,
			"feed-delete-button"));
	g_signal_connect(
		button3,
		"clicked",
		G_CALLBACK(feeds_dialog_delete),
		treeview);


	rf->preferences = GTK_WIDGET (gtk_builder_get_object(gui, "rss-config-control"));
	check1 = GTK_WIDGET (gtk_builder_get_object(gui, "checkbutton1"));
	check2 = GTK_WIDGET (gtk_builder_get_object(gui, "checkbutton2"));
	check3 = GTK_WIDGET (
			gtk_builder_get_object(
				gui,
				"checkbutton3"));
	check4 = GTK_WIDGET (
			gtk_builder_get_object(
				gui,
				"checkbutton4"));
	check5 = GTK_WIDGET (
			gtk_builder_get_object(
				gui,
				"checkbutton5"));
	check6 = GTK_WIDGET (
			gtk_builder_get_object(
				gui,
				"checkbuttonS6"));
	check7 = GTK_WIDGET (
			gtk_builder_get_object(
				gui,
				"checkbutton9"));
	spin = GTK_WIDGET (gtk_builder_get_object(gui, "spinbutton1"));
	enclsize = GTK_WIDGET (gtk_builder_get_object(gui, "spinbutton2"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check1),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(client, GCONF_KEY_REP_CHECK, NULL));
#else
		g_settings_get_boolean(settings, CONF_REP_CHECK));
#endif

#if EVOLUTION_VERSION < 30304
	adj = gconf_client_get_float(client, GCONF_KEY_REP_CHECK_TIMEOUT, NULL);
#else
	adj = g_settings_get_double(settings, CONF_REP_CHECK_TIMEOUT);
#endif
	if (adj)
		gtk_spin_button_set_value((GtkSpinButton *)spin, adj);
	g_signal_connect(
		check1,
		"clicked",
		G_CALLBACK(rep_check_cb),
		spin);
	g_signal_connect(
		spin,
		"value-changed",
		G_CALLBACK(rep_check_timeout_cb),
		check1);

#if EVOLUTION_VERSION < 30304
	size = gconf_client_get_float(client, GCONF_KEY_ENCLOSURE_SIZE, NULL);
#else
	size = g_settings_get_double(settings, CONF_ENCLOSURE_SIZE);
#endif
	if (size)
		gtk_spin_button_set_value((GtkSpinButton *)enclsize, size);
	g_signal_connect(
		check7,
		"clicked",
		G_CALLBACK(enclosure_limit_cb),
		enclsize);
	g_signal_connect(
		enclsize,
		"value-changed",
		G_CALLBACK(enclosure_size_cb),
		check7);

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (check2),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(
			client,
			GCONF_KEY_START_CHECK,
			NULL));
#else
		g_settings_get_boolean(settings, CONF_START_CHECK));
#endif
	g_signal_connect(check2,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_START_CHECK);
#else
		(gpointer)CONF_START_CHECK);
#endif
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (check3),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(
			client,
			GCONF_KEY_DISPLAY_SUMMARY,
			NULL));
#else
		g_settings_get_boolean(settings, CONF_DISPLAY_SUMMARY));
#endif
	g_signal_connect(check3,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_DISPLAY_SUMMARY);
#else
		(gpointer)CONF_DISPLAY_SUMMARY);
#endif
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (check4),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(
			client,
			GCONF_KEY_SHOW_COMMENTS,
			NULL));
#else
		g_settings_get_boolean(settings, CONF_SHOW_COMMENTS));
#endif
	g_signal_connect(check4,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_SHOW_COMMENTS);
#else
		(gpointer)CONF_SHOW_COMMENTS);
#endif
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (check5),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(
			client,
			GCONF_KEY_SEARCH_RSS,
			NULL));
#else
		g_settings_get_boolean(settings, CONF_SEARCH_RSS));
#endif
	g_signal_connect(check5,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_SEARCH_RSS);
#else
		(gpointer)CONF_SEARCH_RSS);
#endif
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (check6),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(
			client,
			GCONF_KEY_DOWNLOAD_ENCLOSURES,
			NULL));
#else
		g_settings_get_boolean(settings, CONF_DOWNLOAD_ENCLOSURES));
#endif
	g_signal_connect(check6,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_DOWNLOAD_ENCLOSURES);
#else
		(gpointer)CONF_DOWNLOAD_ENCLOSURES);
#endif
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (check7),
#if EVOLUTION_VERSION < 30304
		gconf_client_get_bool(
			client,
			GCONF_KEY_ENCLOSURE_LIMIT,
			NULL));
#else
		g_settings_get_boolean(settings, CONF_ENCLOSURE_LIMIT));
#endif
	g_signal_connect(check7,
		"clicked",
		G_CALLBACK(start_check_cb),
#if EVOLUTION_VERSION < 30304
		(gpointer)GCONF_KEY_ENCLOSURE_LIMIT);
#else
		(gpointer)CONF_ENCLOSURE_LIMIT);
#endif

	import = GTK_WIDGET (gtk_builder_get_object(gui, "import"));
	export = GTK_WIDGET (gtk_builder_get_object(gui, "export"));
	g_signal_connect(
		import,
		"clicked",
		G_CALLBACK(import_cb),
		import);
	g_signal_connect(
		export,
		"clicked",
		G_CALLBACK(export_cb),
		export);

	control_widget = GTK_WIDGET (
				gtk_builder_get_object(
					gui,
					"feeds-notebook"));
	g_object_ref (control_widget);

	gtk_container_remove (
		GTK_CONTAINER (gtk_widget_get_parent(control_widget)),
		control_widget);
#if EVOLUTION_VERSION < 30304
	g_object_unref(client);
#else
	g_object_unref(settings);
#endif
	g_object_unref (gui);

	return control_widget;
}

#if EVOLUTION_VERSION < 22900
static BonoboObject *
factory (BonoboGenericFactory *factory,
	const char *component_id,
	void *closure)
{
	g_return_val_if_fail(upgrade == 2, NULL);

	g_print("component_id:%s\n", component_id);

	if (strcmp (component_id, RSS_CONTROL_ID) == 0)
		return BONOBO_OBJECT (rss_config_control_new ());

	g_warning (
		FACTORY_ID ": Don't know what to do with %s",
		component_id);
	return NULL;
}

BONOBO_ACTIVATION_SHLIB_FACTORY (
	FACTORY_ID, "Evolution RSS component factory", factory, NULL)
#endif

#if EVOLUTION_VERSION >= 22900
void
init_rss_prefs(void)
{
	EShell *shell;
	GtkWidget *preferences_window;

	gtk_icon_theme_append_search_path (
		gtk_icon_theme_get_default (),
		EVOLUTION_ICONDIR);

	shell = e_shell_get_default();
	preferences_window = e_shell_get_preferences_window (shell);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"page-rss",
		"rss",
		_("News and Blogs"),
#if EVOLUTION_VERSION >= 30390
		NULL,
#endif
		(EPreferencesWindowCreatePageFn)rss_config_control_new,
		800);
}
#endif

