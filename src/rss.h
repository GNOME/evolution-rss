/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2010  Lucian Langa <cooly@gnome.eu.org>
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
#include "debug.h"

#include "network.h"

#ifndef __RSS_H_
#define __RSS_H_

#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <shell/e-shell.h>
#include <shell/e-shell-view.h>
#if (EVOLUTION_VERSION < 30905)
#include <shell/es-event.h>
#endif

#if (DATASERVER_VERSION >= 2031001)
#include <camel/camel.h>
#else
#include <camel/camel-folder.h>
#include <camel/camel-operation.h>
#endif
#include <mail/em-event.h>

#if (EVOLUTION_VERSION > 30501)
#include <em-format/e-mail-formatter.h>
#else
#include <mail/em-format-html.h>
#if EVOLUTION_VERSION >= 30400
#include <mail/em-format-hook.h>
#endif
#endif
#include <mail/e-mail-display.h>

#ifdef HAVE_WEBKIT
#include <webkit/webkitwebview.h>
#endif

#define PLUGIN_INSTALL_DIR @PLUGIN_INSTALL_DIR@
#define DEFAULT_FEEDS_FOLDER N_("News and Blogs")
#define OLD_FEEDS_FOLDER "News&Blogs"
#define DEFAULT_NO_CHANNEL N_("Untitled channel")
#define DEFAULT_TTL 1800
#define FEED_IMAGE_TTL 604800 /*ohne week*/

/* ms between status updates to the gui */
#ifndef _WIN32
#define STATUS_TIMEOUT (250)
#endif

#define NETWORK_MIN_TIMEOUT (60)
#define NETWORK_TIMEOUT (180000)

typedef struct _RDF {
	gchar		*base;
	char		*uri;
	char		*html;
	xmlDocPtr	cache;
	gboolean	shown;
	gchar		*type;		//char type
	guint		type_id;	//num type
	gchar		*version;	//feed version
	gchar		*feedid;	//md5 string id of feed
	gchar		*title;		//title of the feed
	gchar		*prefix;	//directory path
	gchar		*maindate;	//channel date
	GArray		*item;		//feed content
	gchar		*image;		//feed image
	GtkWidget	*progress;
	guint		total;		//total articles
	guint		ttl;		//feed specified refresh interval
	/* Soup stuff */
	SoupMessage *message;
	guint		error;		//invalid feed
	char		*strerror;	//error msg
	GArray		*uids;
} RDF;

//we keep these values of a feed to be deleted in order
//to easily restore in case delete does not success or
//it is canceled
typedef struct _hrfeed {
	gchar *hrname;		//hr name <name> <key>
	gchar *hrname_r;	//hr name <key> <name>
	gchar *hr;		//hr url <key>, <url>
	guint hre;		//hr enabled <key> <enabled>
	gchar *hrt;		//hr type <key> <>
	guint hrh;		//hr html <key> <>
	guint hrdel_feed;
	guint hrdel_days;
	guint hrdel_messages;
	guint hrdel_unread;
	guint hrdel_notpresent;
	guint hrupdate;
	guint hrttl;
	guint hrttl_multiply;
} hrfeed;

typedef struct _rssfeed {
	GHashTable      *hrname;		//bind feed name to key
	GHashTable      *hrname_r;		//and mirrored structure for faster lookups
	GHashTable      *hrcrc;			//crc32 to key binding
	GHashTable      *hr;			//feeds hash
	GHashTable      *hn;			//feeds hash
	GHashTable      *hre;			//enabled feeds hash
	GHashTable      *hrt;			//feeds name hash
	GHashTable      *hrh;			//fetch html flag
	GHashTable      *hruser;		//auth user hash
	GHashTable      *hrpass;		//auth pass hash
	gboolean	soup_auth_retry;	//wether to retry auth after an unsucessful auth
	GHashTable      *hrdel_feed;		//option to delete messages in current feed
	GHashTable      *hrdel_days;		//option to delete messages older then days
	GHashTable      *hrdel_messages;	//option to keep last messages
	GHashTable      *hrdel_unread;		//option to delete unread messages too
	GHashTable      *hrdel_notpresent;	//option to delete messages that are not present in the feed
	GHashTable      *hrttl;
	GHashTable      *hrttl_multiply;
	GHashTable      *hrupdate;		//feeds update method
	GtkWidget       *feed_dialog;
	GtkWidget       *progress_dialog;
	GtkWidget       *progress_bar;
#if EVOLUTION_VERSION < 30504
	GtkWidget       *label;
#endif
	GtkWidget       *sr_feed;		//s&r upper text (feed)
	GtkWidget       *treeview;
	GtkWidget       *edbutton;
	GtkWidget	*errdialog;
	GtkWidget	*preferences;
	gchar		*err;			//if using soup _unblocking error goes here
	gchar		*err_feed;		//name of the feed that caused above err
	gchar           *cfeed;			//current feed name
	gboolean	online;			//networkmanager dependant
	gboolean	fe;			//feed enabled (at least one)
	EMEventTargetSendReceive *t;
	gboolean        setup;
	gboolean        pending;
	guint		import;			//import going on
	gboolean	import_cancel;		//cancel all active imports going on
	gboolean	display_cancel;		//cancel all active feeds displaying generated by imports
	gboolean	autoupdate;		//feed is currently auto fetched
	guint		feed_queue;
	gboolean        cancel;			//cancelation signal
	gboolean        cancel_all;		//cancelation signal
	GHashTable      *session;		//queue of active unblocking sessions
	GHashTable      *abort_session;		//this is a hack to be able to iterate when
						//we remove keys from seesion with weak_ref
	GHashTable      *key_session;		//queue of active unblocking sessions and keys linked
	SoupSession     *b_session;		//active blocking session
	SoupMessage     *b_msg_session;		//message running in the blocking session
	guint		rc_id;
	struct _send_info *info;		//s&r data
	struct userpass	*un;
	guint		cur_format;
	guint		chg_format;
	guint		headers_mode;		//full/simple headers lame method used for gtkmoz & webkit widget calculation
	GtkWidget	*mozembed;		// object holding gtkmozebmed struct
						// apparently can only be one
	GtkWidget	*moz;
	gchar		*main_folder;		// "News&Blogs" folder name
	GHashTable	*feed_folders;		// defined feeds folders
	GHashTable	*reversed_feed_folders;	// easyer when we lookup for the value
	GHashTable	*activity;
	GHashTable	*error_hash;
	guint		test;
	char		*current_uid;		// currently read article
	GQueue		*stqueue;		// network downloads tracking
	GList		*enclist;		// network downloads tracking
} rssfeed;

#if EVOLUTION_VERSION < 30304
#define GCONF_KEY_DISPLAY_SUMMARY "/apps/evolution/evolution-rss/display_summary"
#define GCONF_KEY_START_CHECK "/apps/evolution/evolution-rss/startup_check"
#define GCONF_KEY_CUSTOM_FONT "/apps/evolution/evolution-rss/custom_font"
#define GCONF_KEY_MIN_FONT_SIZE "/apps/evolution/evolution-rss/min_font_size"
#define GCONF_KEY_REP_CHECK "/apps/evolution/evolution-rss/rep_check"
#define GCONF_KEY_REP_CHECK_TIMEOUT "/apps/evolution/evolution-rss/rep_check_timeout"
#define GCONF_KEY_SHOW_COMMENTS "/apps/evolution/evolution-rss/show_comments"
#define GCONF_KEY_DOWNLOAD_ENCLOSURES "/apps/evolution/evolution-rss/download_enclosures"
#define GCONF_KEY_ENCLOSURE_LIMIT "/apps/evolution/evolution-rss/enclosure_limit"
#define GCONF_KEY_ENCLOSURE_SIZE "/apps/evolution/evolution-rss/enclosure_size"
#define GCONF_KEY_REMOVE_FOLDER "/apps/evolution/evolution-rss/remove_folder"
#define GCONF_KEY_HTML_RENDER "/apps/evolution/evolution-rss/html_render"
#define GCONF_KEY_HTML_JS "/apps/evolution/evolution-rss/html_js"
#define GCONF_KEY_HTML_JAVA "/apps/evolution/evolution-rss/html_java"
#define GCONF_KEY_EMBED_PLUGIN "/apps/evolution/evolution-rss/embed_plugin"
#define GCONF_KEY_STATUS_ICON "/apps/evolution/evolution-rss/status_icon"
#define GCONF_KEY_BLINK_ICON "/apps/evolution/evolution-rss/blink_icon"
#define GCONF_KEY_FEED_ICON "/apps/evolution/evolution-rss/feed_icon"
#define GCONF_KEY_ACCEPT_COOKIES "/apps/evolution/evolution-rss/accept_cookies"
#define GCONF_KEY_IMAGE_RESIZE "/apps/evolution/evolution-rss/image_resize"
#define GCONF_KEY_SEARCH_RSS "/apps/evolution/evolution-rss/search_rss"
#else
#define RSS_CONF_SCHEMA "org.gnome.evolution.plugin.rss"
#define CONF_DISPLAY_SUMMARY "display-summary"
#define CONF_ACCEPT_COOKIES "accept-cookies"
#define CONF_DOWNLOAD_ENCLOSURES "download-enclosures"
#define CONF_ENCLOSURE_LIMIT "enclosure-limit"
#define CONF_ENCLOSURE_SIZE "enclosure-size"
#define CONF_REMOVE_FOLDER "remove-folder"
#define CONF_CUSTOM_FONT "custom-font"
#define CONF_MIN_FONT_SIZE "min-font-size"
#define CONF_HTML_RENDER "html-render"
#define CONF_HTML_JS "html-js"
#define CONF_HTML_JAVA "html-java"
#define CONF_EMBED_PLUGIN "embed-plugin"
#define CONF_STATUS_ICON "status-icon"
#define CONF_BLINK_ICON "blink-icon"
#define CONF_START_CHECK "startup-check"
#define CONF_REP_CHECK "rep-check"
#define CONF_REP_CHECK_TIMEOUT "rep-check-timeout"
#define CONF_IMAGE_RESIZE "image-resize"
#define CONF_SEARCH_RSS "search-rss"
#define CONF_FEED_ICON "feed-icon"
#define CONF_SHOW_COMMENTS "show-comments"
#define CONF_SHOW_XML_ERRORS "show-xml-errors"
#define CONF_SHOW_FEED_ERRORS "show-feed-errors"
#endif

enum {
	RSS_FEED,
	RDF_FEED,
	ATOM_FEED
};

typedef struct ADD_FEED {
	GtkWidget	*dialog;
	GtkWidget	*dialog_aarea;
	GtkWidget	*progress;
	GtkWidget	*child;		//the dialog child
	GtkBuilder	*gui;
	gchar           *feed_url;
	gchar		*feed_name;
	gchar		*prefix;
	gchar		*tmsg;		//status bar message
	gboolean        fetch_html;	//show webpage instead of summary
	gboolean        add;		//ok button
	gboolean	changed;
	gboolean	enabled;
	gboolean	validate;
	guint		del_feed;
	guint		del_days;	// delete messages over del_days old
	guint		del_messages;	// delete all messages but the last del_messages
	gboolean	del_unread;	// delete unread messages too
	gboolean	del_notpresent;	// delete messages that are not present in the feed
	guint		ttl;	// recommended update time
	guint		ttl_multiply;	// how much we multiyply ttl value (minutes)
	guint		update;	// feed update method global; ttl; disabled
	gboolean	renamed;
	gboolean	edit;
	GFunc		ok;
	void		*ok_arg;
	GFunc		cancelable;
	void		*cancelable_arg;
} add_feed;

typedef struct USERPASS {
	gchar *username;
	gchar *password;
} userpass;

struct _send_data {
	GList *infos;

	GtkDialog *gd;
	int cancelled;

	CamelFolder *inbox;     /* since we're never asked to update this one, do it ourselves */
	time_t inbox_update;

	GMutex *lock;
	GHashTable *folders;

	GHashTable *active;     /* send_info's by uri */
};

typedef enum {
	SEND_RECEIVE,           /* receiver */
	SEND_SEND,              /* sender */
	SEND_UPDATE,            /* imap-like 'just update folder info' */
	SEND_INVALID
} send_info_t ;

typedef enum {
	SEND_ACTIVE,
	SEND_CANCELLED,
	SEND_COMPLETE
} send_state_t;

struct _send_info {
	send_info_t type;               /* 0 = fetch, 1 = send */
#if (DATASERVER_VERSION >= 2033001)
	GCancellable *cancel;
#else
	CamelOperation *cancel;
#endif
	char *uri;
	int keep;
	send_state_t state;
	GtkWidget *progress_bar;
	GtkWidget *cancel_button;
#if EVOLUTION_VERSION < 30504
	GtkWidget *status_label;
#endif

	int again;              /* need to run send again */

	int timeout_id;
	char *what;
	int pc;

	/*time_t update;*/
	struct _send_data *data;
};

typedef struct CREATE_FEED {	/* used by create_mail function when called by unblocking fetch */
	gchar *feed;
	gchar *full_path;	// news&blogs path
	gchar 	*q,
		*sender,	// author
		*subj,		// subject
		*body,		// body
		*date,		// date
		*dcdate,	// dublin core date
		*website;	// article's webpage
	gchar	*feedid;
	gchar	*feed_fname;	// feed name file
	gchar	*feed_uri;
	gchar *encl;		//feed enclosure
	gchar *enclurl;
	GList *attachments;	//feed media files
	GHashTable *attlengths;	//feed media files
	GList *attachedfiles;	//list of downloaded media files
	guint attachmentsqueue;	//list of downloaded media files
	FILE *efile;		//enclosure file
	gchar *comments;
	GList *category;	// list of categories article is posted under
} create_feed;

typedef struct rss_auth {
	gchar *url;
	gchar *user;
	gchar *pass;
	SoupAuth *soup_auth;
	SoupSession *session;
	SoupMessage *message;
	gboolean retrying;
	GtkWidget *username;
	GtkWidget *password;
	GtkWidget *rememberpass;
} RSS_AUTH;

struct _rfMessage {
	guint    status_code;
	gchar   *body;
	goffset  length;
};
typedef struct _rfMessage rfMessage;

void compare_enabled(
	gpointer key,
	gpointer value,
	guint *data);
guint rss_find_enabled(void);
void error_destroy(GObject *o, void *data);
void error_response(
	GObject *o,
	int button,
	void *data);
void abort_active_op(gpointer key);
void cancel_active_op(gpointer key);
void browser_write(
	gchar *string,
	gint length,
	gchar *base);
void user_pass_cb(
	RSS_AUTH *auth_info,
	gint response,
	GtkDialog *dialog);

gboolean proxy_auth_dialog(
	gchar *title,
	gchar *user,
	gchar *pass);

gboolean timeout_soup(void);
void network_timeout(void);
void prepare_feed(
	gpointer key,
	gpointer value,
	gpointer user_data);
void reload_cb (GtkWidget *button, gpointer data);
void gecko_set_preferences(void);
void browser_copy_selection(
	GtkWidget *widget,
	gpointer data);
void browser_select_all(GtkWidget *widget, gpointer data);
void webkit_set_preferences(void);
#ifdef HAVE_WEBKIT
gboolean webkit_over_link(
	WebKitWebView *web_view,
	gchar         *title,
	gchar         *uri,
	gpointer       user_data);

gboolean
webkit_click (
	GtkEntry *entry,
	GtkMenu *menu,
	gpointer user_data);
#endif
GtkDialog* create_user_pass_dialog(RSS_AUTH *auth);
void err_destroy (
	GtkWidget *widget,
	guint response,
	gpointer data);
void save_gconf_feed(void);
void rss_error(
	gpointer key,
	gchar *name,
	gchar *error,
	gchar *emsg);
void rss_hooks_init(void);
void rss_select_folder(gchar *folder_name);
gboolean update_articles(gboolean disabler);

gchar *lookup_chn_name_by_url(gchar *url);
gchar *lookup_main_folder(void);
gchar *lookup_feed_folder(gchar *folder);
gchar *lookup_feed_folder_raw(gchar *folder);
gchar *lookup_original_folder(
	gchar *folder,
	gboolean *found);
gchar *lookup_uri_by_folder_name(gchar *name);

gchar *decode_utf8_entities(gchar *str);
gchar *decode_html_entities(gchar *str);
gchar *get_real_channel_name(gchar *uri, gchar *failed);
gchar *fetch_image(gchar *url, gchar *link);
gchar *fetch_image_redraw(gchar *url, gchar *link, gpointer data);
void create_mail(create_feed *CF);
void free_cf(create_feed *CF);
gchar *generate_safe_chn_name(gchar *chn_name);
void update_sr_message(void);
void update_progress_text(gchar *title);
void update_feed_image(RDF *r);
void fetch_comments(gchar *url, gchar *mainurl, EMailDisplay *stream);
gchar *print_comments(gchar *url, gchar *stream, EMailFormatter *format);
void cancel_comments_session(SoupSession *sess);
gchar *search_rss(char *buffer, int len);
void prepare_hashes(void);
void update_ttl(gpointer key, guint value);
void refresh_mail_folder(CamelFolder *mail_folder);
gboolean check_chn_name(gchar *chn_name);
void
#if LIBSOUP_VERSION < 2003000
finish_website (SoupMessage *msg, gpointer user_data);
#else
finish_website (
	SoupSession *soup_sess,
	SoupMessage *msg,
	gpointer user_data);
#endif

void
#if LIBSOUP_VERSION < 2003000
finish_enclosure (
	SoupMessage *msg,
	create_feed *user_data);
#else
finish_enclosure (
	SoupSession *soup_sess,
	SoupMessage *msg,
	create_feed *user_data);
#endif
void
#if LIBSOUP_VERSION < 2003000
finish_feed (SoupMessage *msg, gpointer user_data);
#else
finish_feed (
	SoupSession *soup_sess,
	SoupMessage *msg,
	gpointer user_data);
#endif
void generic_finish_feed(rfMessage *msg, gpointer user_data);
//void textcb(NetStatusType status, gpointer statusdata, gpointer data);

void download_chunk(
	NetStatusType status,
	gpointer statusdata,
	gpointer data);

gboolean process_enclosure(create_feed *CF);
gboolean process_attachments(create_feed *CF);

#ifdef HAVE_GECKO
void rss_mozilla_init(void);
#endif
void write_feeds_folder_line(
	gpointer key,
	gpointer value,
	FILE *file);
void populate_reversed(
	gpointer key,
	gpointer value,
	GHashTable *hash);
CamelStore *rss_component_peek_local_store(void);
void custom_feed_timeout(void);
CamelFolder *check_feed_folder(gchar *folder_name);
gboolean setup_feed(add_feed *feed);
void web_auth_dialog(RSS_AUTH *auth_info);
gpointer lookup_key(gpointer key);
void rss_delete_feed(gchar *name, gboolean folder);
gint update_feed_folder(
	gchar *old_name,
	gchar *new_name,
	gboolean valid_folder);
void
#if LIBSOUP_VERSION < 2003000
finish_update_feed_image (SoupMessage *msg, gpointer user_data);
#else
finish_update_feed_image (
	SoupSession *soup_sess,
	SoupMessage *msg,
	gpointer user_data);
#endif
void rss_finalize(void);
gboolean check_update_feed_image(gchar *key);
void update_main_folder(gchar *new_name);
void search_rebase(gpointer key, gpointer value, gchar *oname);
void evo_window_popup(GtkWidget *window);
gboolean check_if_enabled (
	gpointer key,
	gpointer value,
	gpointer user_data);
void free_filter_uids (gpointer user_data, GObject *ex_msg);
#if EVOLUTION_VERSION >= 22900
void quit_cb(void *ep, EShellView *shell_view);
EShellView *rss_get_mail_shell_view (gboolean with_mail_reader);
#endif
void rebase_feeds(gchar *old_name, gchar *new_name);

void rss_inc_farticle (void);
void rss_inc_ftotal (void);

#ifdef _WIN32
char *strcasestr(const char *a, const char *b);

const char *_e_get_uidir (void) G_GNUC_CONST;
const char *_e_get_imagesdir (void) G_GNUC_CONST;

#undef EVOLUTION_UIDIR
#define EVOLUTION_UIDIR _e_get_uidir ()

#undef EVOLUTION_ICONDIR
#define EVOLUTION_ICONDIR _e_get_imagesdir ()

#endif
#endif
