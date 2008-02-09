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

#if HAVE_DBUS
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#endif

#ifndef __RSS_H_
#define __RSS_H_

#define PLUGIN_INSTALL_DIR @PLUGIN_INSTALL_DIR@

GConfClient *rss_gconf;
GSList *rss_list = NULL;

typedef struct _RDF {
        char 		*uri;
        char 		*html;
        xmlDocPtr 	cache;
        gboolean 	shown;
        gchar 		*type;    	//char type
        guint 		type_id; 	//num type
	gchar		*version;	//feed version
        gchar		*feedid;  	//md5 string id of feed
        /* Soup stuff */
        SoupMessage *message;
} RDF;

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
        GHashTable      *hrpass;   		//auth user hash
	gboolean	soup_auth_retry;	//wether to retry auth after an unsucessful auth
        GHashTable      *hrdel_feed;   		//option to delete messages in current feed
        GHashTable      *hrdel_days;   		//option to delete messages older then days
        GHashTable      *hrdel_messages; 	//option to keep last messages
        GHashTable      *hrdel_unread; 		//option to delete unread messages too
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
	guint		feed_queue;
        gboolean        cancel; 		//cancelation signal
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
#if HAVE_DBUS
	DBusConnection	*bus;			// DBUS
#endif
} rssfeed;

#define GCONF_KEY_DISPLAY_SUMMARY "/apps/evolution/evolution-rss/display_summary"
#define GCONF_KEY_START_CHECK "/apps/evolution/evolution-rss/startup_check"
#define GCONF_KEY_REP_CHECK "/apps/evolution/evolution-rss/rep_check"
#define GCONF_KEY_REP_CHECK_TIMEOUT "/apps/evolution/evolution-rss/rep_check_timeout"
#define GCONF_KEY_USE_PROXY "/apps/evolution/evolution-rss/use_proxy"
#define GCONF_KEY_HOST_PROXY "/apps/evolution/evolution-rss/host_proxy"
#define GCONF_KEY_PORT_PROXY "/apps/evolution/evolution-rss/port_proxy"
#define GCONF_KEY_AUTH_PROXY "/apps/evolution/evolution-rss/auth_proxy"
#define GCONF_KEY_USER_PROXY "/apps/evolution/evolution-rss/user_proxy"
#define GCONF_KEY_PASS_PROXY "/apps/evolution/evolution-rss/pass_proxy"
#define GCONF_KEY_REMOVE_FOLDER "/apps/evolution/evolution-rss/remove_folder"
#define GCONF_KEY_HTML_RENDER "/apps/evolution/evolution-rss/html_render"

enum {
	RSS_FEED,
	RDF_FEED,
	ATOM_FEED
};

typedef struct ADD_FEED {
	GtkWidget	*dialog;
        gchar           *feed_url;
	gchar		*feed_name;
        gboolean        fetch_html;	//show webpage instead of summary
        gboolean        add;		//ok button
	gboolean	changed;
	gboolean	enabled;
	gboolean	validate;
	guint		del_feed;
	guint		del_days;	// delete messages over del_days old
	guint		del_messages;	// delete all messages but the last del_messages
	gboolean	del_unread;	// delete unread messages too
} add_feed;

typedef struct USERPASS {
	gchar *username;
	gchar *password;
} userpass;

typedef struct _setupfeed {
	GladeXML  *gui;
	GtkWidget *treeview;
	GtkWidget *add_feed;
	GtkWidget *check1;
	GtkWidget *check2;
	GtkWidget *check3;
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
} create_feed;

guint count = 0;
gchar *buffer = NULL;

guint           upgrade = 0;                // set to 2 when initailization successfull

u_int32_t gen_crc(const char *msg);
gboolean create_user_pass_dialog(gchar *url);
static void start_check_cb (GtkWidget *widget, gpointer data);
static void err_destroy (GtkWidget *widget, guint response, gpointer data);
static gboolean check_if_match (gpointer key, gpointer value, gpointer user_data);
void save_gconf_feed(void);
void rss_error(gpointer key, gchar *name, gchar *error, gchar *emsg);
void rss_select_folder(gchar *folder_name);
gpointer lookup_chn_name_by_url(gchar *url);
gboolean update_articles(gboolean disabler);
static xmlNode *html_find (xmlNode *node, char *match);
gchar *lookup_main_folder(void);
gchar *lookup_feed_folder(gchar *folder);
gchar *decode_html_entities(gchar *str);
#ifdef HAVE_GTKMOZEMBED
void rss_mozilla_init(void);
#endif
gpointer lookup_key(gpointer key);
void taskbar_op_set_progress(gpointer key, gdouble progress);
void taskbar_op_finish(gpointer key);
void taskbar_push_message(gchar *message);
void taskbar_pop_message(void);
void write_feeds_folder_line(gpointer key, gpointer value, FILE *file);
void populate_reversed(gpointer key, gpointer value, GHashTable *hash);



typedef struct FEED_FOLDERS {
	gchar *oname;		//original folder name
	gchar *rname;		// renamed folder name
} feed_folders;

#endif
