/*  Evolution RSS Reader Plugin
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
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#if (DATASERVER_VERSION > 3005001)
#include <libedataserver/libedataserver.h>
#else
#if (DATASERVER_VERSION >= 2023001)
#include <libedataserver/e-proxy.h>
#endif
#endif

#include <libsoup/soup.h>
#ifdef HAVE_LIBSOUP_GNOME
#include <libsoup/soup-gnome.h>
#include <libsoup/soup-gnome-features.h>
#endif

void abort_all_soup(void);
gboolean abort_soup_sess(
	gpointer key,
	gpointer value,
	gpointer user_data);

gboolean net_get_unblocking(
	gchar *url,
	NetStatusCallback cb,
	gpointer data,
	gpointer cb2,
	gpointer cbdata2,
	guint track,
	GError **err);

gboolean download_unblocking(
	gchar *url,
	NetStatusCallback cb,
	gpointer data,
	gpointer cb2,
	gpointer cbdata2,
	guint track,
	GError **err);

GString *net_post_blocking(
	gchar *url,
	GSList *headers,
	GString *post,
	NetStatusCallback cb,
	gpointer data,
	GError **err);

typedef void (*wkCallback)(gchar *str, gchar *base, gchar *encoding);

typedef struct {
	gchar *str;
	wkCallback cb;
	gchar *base;
	gchar *host;
	gchar *encoding;
	SoupAddress *addr;
} WEBKITNET;


#define NET_ERROR net_error_quark()
int net_error_quark(void);
gboolean net_queue_dispatcher(void);

#if (DATASERVER_VERSION >= 2023001)
EProxy *proxy_init(void);
void proxify_webkit_session(EProxy *proxy, gchar *uri);
void proxify_session(EProxy *proxy, SoupSession *session, gchar *uri);
void proxify_webkit_session_async(EProxy *proxy, WEBKITNET *wknet);
#endif

guint save_up(gpointer data);
guint del_up(gpointer data);
void rss_soup_init(void);
guint read_up(gpointer data);
void sync_gecko_cookies(void);

