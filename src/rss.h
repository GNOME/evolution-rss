/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2009  Lucian Langa <cooly@gnome.eu.org> 
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

#if HAVE_DBUS
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#endif
#include "network.h"

#ifndef __RSS_H_
#define __RSS_H_

#include <libsoup/soup.h>
#include <mail/mail-component.h>

#include <glade/glade.h>
#include <mail/em-event.h>
#include <camel/camel-folder.h>
#include <camel/camel-operation.h>

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
#define HTTP_CACHE_PATH "http"

typedef struct _RDF {
        char 		*uri;
        char 		*html;
        xmlDocPtr 	cache;
        gboolean 	shown;
        gchar 		*type;    	//char type
        guint 		type_id; 	//num type
	gchar		*version;	//feed version
        gchar		*feedid;  	//md5 string id of feed
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
	gchar *hrname;
	gchar *hrname_r;
	gchar *hr;
	guint hre;
	gchar *hrt;
	guint hrh;
	guint hrdel_feed;
	guint hrdel_days;
	guint hrdel_messages;
	guint hrdel_unread;
	guint hrupdate;
	guint hrttl;
	guint hrttl_multiply;
} hrfeed;

typedef struct _rssfeed {
        GHashTable      *hrname;            	//bind feed name to key
        GHashTable      *hrname_r;            	//and mirrored structure for faster lookups
        GHashTable      *hrcrc;            	//crc32 to key binding
        GHashTable      *hr;            	//feeds hash
        GHashTable      *hn;            	//feeds hash
        GHashTable      *hre;   		//enabled feeds hash
        GHashTable      *hrt;   		//feeds name hash
        GHashTable      *hrh;   		//fetch html flag
        GHashTable      *hruser;   		//auth user hash
        GHashTable      *hrpass;   		//auth pass hash
	gboolean	soup_auth_retry;	//wether to retry auth after an unsucessful auth
        GHashTable      *hrdel_feed;   		//option to delete messages in current feed
        GHashTable      *hrdel_days;   		//option to delete messages older then days
        GHashTable      *hrdel_messages; 	//option to keep last messages
        GHashTable      *hrdel_unread; 		//option to delete unread messages too
        GHashTable      *hrttl;   		
        GHashTable      *hrttl_multiply;	
        GHashTable      *hrupdate;   		//feeds update method
        GtkWidget       *feed_dialog;
        GtkWidget       *progress_dialog;
        GtkWidget       *progress_bar;
        GtkWidget       *label;
        GtkWidget       *sr_feed;		//s&r upper text (feed)
        GtkWidget       *treeview;
        GtkWidget       *edbutton;
	GtkWidget	*errdialog;
	GtkWidget	*preferences;
	gchar		*err;			//if using soup _unblocking error goes here
	gchar		*err_feed;		//name of the feed that caused above err
        gchar           *cfeed; 		//current feed name
	gboolean	online;			//networkmanager dependant
	gboolean	fe;			//feed enabled (at least one)
#ifdef EVOLUTION_2_12
	EMEventTargetSendReceive *t;
#else
        EMPopupTargetSelect *t;
#endif
        gboolean        setup;
        gboolean        pending;
        gboolean        import;			//import going on
	gboolean 	autoupdate;		//feed is currently auto fetched
	guint		feed_queue;
        gboolean        cancel; 		//cancelation signal
        gboolean        cancel_all; 		//cancelation signal
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
	gchar		*main_folder;		// "News&Blogs" folder name
	GHashTable	*feed_folders;		// defined feeds folders
	GHashTable	*reversed_feed_folders;	// easyer when we lookup for the value
	GHashTable	*activity;
	GHashTable	*error_hash;
	guint		test;
	char 		*current_uid;		// currently read article	
#if HAVE_DBUS
	DBusConnection	*bus;			// DBUS
#endif
} rssfeed;

#define GCONF_KEY_DISPLAY_SUMMARY "/apps/evolution/evolution-rss/display_summary"
#define GCONF_KEY_START_CHECK "/apps/evolution/evolution-rss/startup_check"
#define GCONF_KEY_REP_CHECK "/apps/evolution/evolution-rss/rep_check"
#define GCONF_KEY_REP_CHECK_TIMEOUT "/apps/evolution/evolution-rss/rep_check_timeout"
#define GCONF_KEY_SHOW_COMMENTS "/apps/evolution/evolution-rss/show_comments"
#define GCONF_KEY_REMOVE_FOLDER "/apps/evolution/evolution-rss/remove_folder"
#define GCONF_KEY_HTML_RENDER "/apps/evolution/evolution-rss/html_render"
#define GCONF_KEY_HTML_JS "/apps/evolution/evolution-rss/html_js"
#define GCONF_KEY_HTML_JAVA "/apps/evolution/evolution-rss/html_java"
#define GCONF_KEY_STATUS_ICON "/apps/evolution/evolution-rss/status_icon"
#define GCONF_KEY_BLINK_ICON "/apps/evolution/evolution-rss/blink_icon"
#define GCONF_KEY_FEED_ICON "/apps/evolution/evolution-rss/feed_icon"
#define GCONF_KEY_ACCEPT_COOKIES "/apps/evolution/evolution-rss/accept_cookies"

enum {
	RSS_FEED,
	RDF_FEED,
	ATOM_FEED
};

typedef struct ADD_FEED {
	GtkWidget	*dialog;
	GtkWidget	*progress;
	GtkWidget	*child;		//the dialog child
	GladeXML	*gui;
        gchar           *feed_url;
	gchar		*feed_name;
	gchar 		*prefix;
        gboolean        fetch_html;	//show webpage instead of summary
        gboolean        add;		//ok button
	gboolean	changed;
	gboolean	enabled;
	gboolean	validate;
	guint		del_feed;
	guint		del_days;	// delete messages over del_days old
	guint		del_messages;	// delete all messages but the last del_messages
	gboolean	del_unread;	// delete unread messages too
	guint		ttl;	// recommended update time
	guint		ttl_multiply;	// how much we multiyply ttl value (minutes)
	guint		update;	// feed update method global; ttl; disabled
	gboolean	renamed;
	gboolean	edit;
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
        CamelOperation *cancel;
        char *uri;
        int keep;
        send_state_t state;
        GtkWidget *progress_bar;
        GtkWidget *cancel_button;
        GtkWidget *status_label;

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
	gchar 	*q,*sender,	// author
		*subj,		// subject
		*body,		// body
		*date,		// date
		*dcdate,	// dublin core date
		*website;	// article's webpage
	gchar	*feedid;
	gchar	*feed_fname;	// feed name file
	gchar	*feed_uri;
	gchar *encl;
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
guint ftotal;
guint farticle;

GtkDialog* create_user_pass_dialog(RSS_AUTH *auth);
void err_destroy (GtkWidget *widget, guint response, gpointer data);
void save_gconf_feed(void);
void rss_error(gpointer key, gchar *name, gchar *error, gchar *emsg);
void rss_select_folder(gchar *folder_name);
gchar *lookup_chn_name_by_url(gchar *url);
gboolean update_articles(gboolean disabler);
xmlNode *html_find (xmlNode *node, char *match);
gchar *lookup_main_folder(void);
gchar *lookup_feed_folder(gchar *folder);
gchar *lookup_original_folder(gchar *folder);
gchar *decode_utf8_entities(gchar *str);
gchar *decode_html_entities(gchar *str);
gchar *get_real_channel_name(gchar *uri, gchar *failed);
gchar *fetch_image(gchar *url, gchar *link);
void create_mail(create_feed *CF);
void migrate_crc_md5(const char *name, gchar *url);
void free_cf(create_feed *CF);
gchar *generate_safe_chn_name(gchar *chn_name);
void update_sr_message(void);
void update_feed_image(RDF *r);
void update_status_icon(const char *channel, gchar *title);
void
#if LIBSOUP_VERSION < 2003000
finish_website (SoupMessage *msg, gpointer user_data);
#else
finish_website (SoupSession *soup_sess, SoupMessage *msg, gpointer user_data);
#endif
void
#if LIBSOUP_VERSION < 2003000
finish_enclosure (SoupMessage *msg, create_feed *user_data);
#else
finish_enclosure (SoupSession *soup_sess, SoupMessage *msg, create_feed *user_data);
#endif
void generic_finish_feed(rfMessage *msg, gpointer user_data);
void textcb(NetStatusType status, gpointer statusdata, gpointer data);
#ifdef HAVE_GECKO
void rss_mozilla_init(void);
#endif
void taskbar_op_set_progress(gpointer key, gchar *msg, gdouble progress);
void taskbar_op_finish(gpointer key);
void taskbar_push_message(gchar *message);
void taskbar_pop_message(void);
void write_feeds_folder_line(gpointer key, gpointer value, FILE *file);
void populate_reversed(gpointer key, gpointer value, GHashTable *hash);
gchar *rss_component_peek_base_directory(MailComponent *component);
void custom_feed_timeout(void);
CamelFolder *check_feed_folder(gchar *folder_name);
gboolean setup_feed(add_feed *feed);
void web_auth_dialog(RSS_AUTH *auth_info);
gchar *get_main_folder(void);
gpointer lookup_key(gpointer key);
void rss_delete_feed(gchar *name, gboolean folder);
void update_feed_folder(gchar *old_name, gchar *new_name);

#ifdef _WIN32
char *strcasestr(const char *a, const char *b);

const char *_e_get_gladedir (void) G_GNUC_CONST;
const char *_e_get_imagesdir (void) G_GNUC_CONST;

#undef EVOLUTION_GLADEDIR
#define EVOLUTION_GLADEDIR _e_get_gladedir ()

#undef EVOLUTION_ICONDIR
#define EVOLUTION_ICONDIR _e_get_imagesdir ()

#endif

typedef struct FEED_FOLDERS {
	gchar *oname;		//original folder name
	gchar *rname;		// renamed folder name
} feed_folders;


#endif
