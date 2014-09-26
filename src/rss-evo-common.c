/* Evoution RSS Reader Plugin
 * Copyright (C) 2011  Lucian Langa <lucilanga@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#if (EVOLUTION_VERSION < 30303)
#include <mail/e-mail-local.h>
#endif
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
#include <mail/e-mail-reader.h>
#include <mail/em-folder-utils.h>

#if (DATASERVER_VERSION > 3005001)
#include <libedataserver/libedataserver.h>
#else
#if (DATASERVER_VERSION >= 2023001)
#include <libedataserver/e-proxy.h>
#endif
#endif

#include <camel/camel.h>

#ifdef HAVE_LIBSOUP_GNOME
#include <libsoup/soup-gnome.h>
#include <libsoup/soup-gnome-features.h>
#endif

#ifdef G_OS_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef HAVE_WSPIAPI_H
#include <wspiapi.h>
#endif

#define IN6_ARE_ADDR_EQUAL(a,b) IN6_ADDR_EQUAL(a,b)

#endif

#define d(x)

#include "rss-evo-common.h"

enum ProxyType {
	PROXY_TYPE_SYSTEM = 0,
	PROXY_TYPE_NO_PROXY,
	PROXY_TYPE_MANUAL,
	PROXY_TYPE_AUTO_URL /* no auto-proxy at the moment */
};

/* Enum definition is copied from gnome-vfs/modules/http-proxy.c */
typedef enum {
        PROXY_IPV4 = 4,
        PROXY_IPV6 = 6
} ProxyAddrType;

typedef struct {
	ProxyAddrType type;     /* Specifies whether IPV4 or IPV6 */
	gpointer  addr;         /* Either in_addr* or in6_addr* */
	gpointer  mask;         /* Either in_addr* or in6_addr* */
} ProxyHostAddr;

struct _EProxyPrivate {
	SoupURI *uri_http, *uri_https;
	guint notify_id_evo, notify_id_sys, notify_id_sys_http; /* conxn id of gconf_client_notify_add  */
	GSList* ign_hosts;      /* List of hostnames. (Strings)         */
	GSList* ign_addrs;      /* List of hostaddrs. (ProxyHostAddrs)  */
	gboolean use_proxy;     /* Is our-proxy enabled? */
	enum ProxyType type;
};


gboolean
rss_emfu_is_special_local_folder (const gchar *name)
{
	return (!strcmp (name, "Drafts") || !strcmp (name, "Inbox") || !strcmp (name, "Outbox") || !strcmp (name, "Sent") || !strcmp (name, "Templates"));
}

void
rss_emfu_copy_folder_selected (EMailBackend *backend,
			const gchar *uri,
			gpointer data)
{
	EMailSession *session;
	struct _copy_folder_data *cfd = data;
	CamelStore *tostore = NULL;
	CamelStore *local_store;
	CamelService *service = NULL;
	CamelProvider *provider;
	const gchar *tobase = NULL;
	CamelURL *url;
	GError *local_error = NULL;

	if (uri == NULL)
		goto fail;

	session = e_mail_backend_get_session (backend);
#if (EVOLUTION_VERSION < 30303)
	local_store = e_mail_local_get_store ();
#else
	local_store = e_mail_session_get_local_store (session);
#endif

	service = CAMEL_SERVICE (cfd->source_store);

#if EVOLUTION_VERSION >= 30501
	camel_service_connect_sync (service, NULL, &local_error);
#else
	camel_service_connect_sync (service, &local_error);
#endif
	if (local_error != NULL) {
#if EVOLUTION_VERSION >= 30303
		e_alert_submit (
			e_mail_backend_get_alert_sink (backend),
			cfd->delete ?
				"mail:no-move-folder-notexist" :
				"mail:no-copy-folder-notexist",
			cfd->source_folder_name, uri,
			local_error->message, NULL);
#else
		e_mail_backend_submit_alert (
			backend, cfd->delete ?
				"mail:no-move-folder-notexist" :
				"mail:no-copy-folder-notexist",
			cfd->source_folder_name, uri,
			local_error->message, NULL);
#endif
		goto fail;
	}

	g_return_if_fail (CAMEL_IS_STORE (service));

	if (cfd->delete && cfd->source_store == local_store &&
		rss_emfu_is_special_local_folder (cfd->source_folder_name)) {
#if EVOLUTION_VERSION >= 30303
		e_alert_submit (
			e_mail_backend_get_alert_sink (backend),
			"mail:no-rename-special-folder",
			cfd->source_folder_name, NULL);
#else
		e_mail_backend_submit_alert (
			backend, "mail:no-rename-special-folder",
			cfd->source_folder_name, NULL);
#endif
		goto fail;
	}

	url = camel_url_new (uri, &local_error);
	if (url != NULL) {
#if EVOLUTION_VERSION > 30505
		service = camel_session_ref_service_by_url(
			CAMEL_SESSION (session), url, CAMEL_PROVIDER_STORE);
#else
		service = camel_session_get_service_by_url (
			CAMEL_SESSION (session), url, CAMEL_PROVIDER_STORE);
#endif

		camel_url_free (url);
	}

	if (service != NULL)
#if EVOLUTION_VERSION >= 30501
		camel_service_connect_sync (service, NULL, &local_error);
#else
		camel_service_connect_sync (service, &local_error);
#endif

	if (local_error != NULL) {
#if EVOLUTION_VERSION >= 30303
		e_alert_submit (
			e_mail_backend_get_alert_sink (backend),
			cfd->delete ?
				"mail:no-move-folder-to-notexist" :
				"mail:no-copy-folder-to-notexist",
			cfd->source_folder_name, uri,
			local_error->message, NULL);
#else
		e_mail_backend_submit_alert (
			backend, cfd->delete ?
				"mail:no-move-folder-to-notexist" :
				"mail:no-copy-folder-to-notexist",
			cfd->source_folder_name, uri,
			local_error->message, NULL);
#endif
		goto fail;
	}

	g_return_if_fail (CAMEL_IS_STORE (service));

	tostore = CAMEL_STORE (service);
	provider = camel_service_get_provider (service);

	url = camel_url_new (uri, NULL);
	if (provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
		tobase = url->fragment;
	else if (url->path && url->path[0])
		tobase = url->path+1;
	if (tobase == NULL)
		tobase = "";

	em_folder_utils_copy_folders (
		cfd->source_store, cfd->source_folder_name,
		tostore, tobase, cfd->delete);

	camel_url_free (url);
fail:

	g_clear_error (&local_error);

	g_free (cfd);
}

void
rss_ipv6_network_addr (const struct in6_addr *addr, const struct in6_addr *mask,
			struct in6_addr *res)
{
	gint i;

	for (i = 0; i < 16; ++i) {
		res->s6_addr[i] = addr->s6_addr[i] & mask->s6_addr[i];
	}
}

gboolean
rss_ep_need_proxy_http (EProxy* proxy, const gchar * host, SoupAddress *addr)
{
	SoupAddress *myaddr = NULL;
	EProxyPrivate *priv = proxy->priv;
	ProxyHostAddr *p_addr = NULL;
	GSList *l;
	guint status;

	/* check for ignored first */
	if (rss_ep_is_in_ignored (proxy, host))
		return FALSE;

	myaddr = soup_address_new (host, 0);
        status = soup_address_resolve_sync (myaddr, NULL);
        if (status == SOUP_STATUS_OK) {
		gint addr_len;
		struct sockaddr* so_addr = NULL;

#ifdef HAVE_LIBSOUP_GNOME
	so_addr = soup_address_get_sockaddr (myaddr, &addr_len);
#endif

	if (!so_addr)
		g_object_unref (myaddr);
		return TRUE;

	if (so_addr->sa_family == AF_INET) {
		struct in_addr in, *mask, *addr_in;

		in = ((struct sockaddr_in *)so_addr)->sin_addr;
		for (l = priv->ign_addrs; l; l = l->next) {
			p_addr = (ProxyHostAddr *)l->data;
			if (p_addr->type == PROXY_IPV4) {
				addr_in =  ((struct in_addr *)p_addr->addr);
				mask = ((struct in_addr *)p_addr->mask);
				d(g_print ("ep_need_proxy:ipv4: in: %ul\t mask: %ul\t addr: %ul\n",
					   in.s_addr, mask->s_addr, addr_in->s_addr));
				if ((in.s_addr & mask->s_addr) == addr_in->s_addr) {
					d(g_print ("Host [%s] doesn't require proxy\n", host));
					return FALSE;
				}
			}
		}
	} else {
		struct in6_addr in6, net6;
		struct in_addr *addr_in, *mask;

		in6 = ((struct sockaddr_in6 *)so_addr)->sin6_addr;
		for (l = priv->ign_addrs; l; l = l->next) {
			p_addr = (ProxyHostAddr *)l->data;
			rss_ipv6_network_addr (&in6, (struct in6_addr *)p_addr->mask, &net6);
			if (p_addr->type == PROXY_IPV6) {
				if (IN6_ARE_ADDR_EQUAL (&net6, (struct in6_addr *)p_addr->addr)) {
					d(g_print ("Host [%s] doesn't require proxy\n", host));
					return FALSE;
				}
			} else if (p_addr->type == PROXY_IPV6 &&
				   IN6_IS_ADDR_V4MAPPED (&net6)) {
				guint32 v4addr;

				addr_in =  ((struct in_addr *)p_addr->addr);
				mask = ((struct in_addr *)p_addr->mask);

				v4addr = net6.s6_addr[12] << 24
					| net6.s6_addr[13] << 16
					| net6.s6_addr[14] << 8
					| net6.s6_addr[15];
				if ((v4addr & mask->s_addr) != addr_in->s_addr) {
					d(g_print ("Host [%s] doesn't require proxy\n", host));
					return FALSE;
				}
			}
		}
	}
	}

	d(g_print ("%s needs a proxy to connect to internet\n", host));
	g_object_unref (myaddr);
	return TRUE;
}

gboolean
rss_ep_need_proxy_https (EProxy* proxy, const gchar * host)
{
	/* Can we share ignore list from HTTP at all? */
	return !rss_ep_is_in_ignored (proxy, host);
}

gboolean
rss_ep_is_in_ignored (EProxy *proxy, const gchar *host)
{
	EProxyPrivate *priv;
	GSList* l;
	gchar *hn;

	g_return_val_if_fail (proxy != NULL, FALSE);
	g_return_val_if_fail (host != NULL, FALSE);

	priv = proxy->priv;
	if (!priv->ign_hosts)
		return FALSE;

	hn = g_ascii_strdown (host, -1);

        for (l = priv->ign_hosts; l; l = l->next) {
                if (*((gchar *)l->data) == '*') {
                        if (g_str_has_suffix (hn, ((gchar *)l->data)+1)) {
                                g_free (hn);
                                return TRUE;
                        }
                } else if (strcmp (hn, l->data) == 0) {
                                g_free (hn);
                                return TRUE;
                }
        }
        g_free (hn);

        return FALSE;
}
