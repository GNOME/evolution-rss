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

#ifndef __RSS_EVO_COMMON_H_
#define __RSS_EVO_COMMON_H_

#include <mail/e-mail-backend.h>
#include <camel/camel.h>
#include <libedataserver/e-proxy.h>

struct _copy_folder_data {
#if EVOLUTION_VERSION >= 30101
	CamelStore *source_store;
	gchar *source_folder_name;
#else
	CamelFolderInfo *fi;
#endif
	gboolean delete;
};

void
rss_ipv6_network_addr (const struct in6_addr *addr,
		const struct in6_addr *mask,
		struct in6_addr *res);

gboolean
rss_emfu_is_special_local_folder (const gchar *name);

void
rss_emfu_copy_folder_selected (EMailBackend *backend,
		const gchar *uri,
		gpointer data);

gboolean
rss_e_proxy_require_proxy_for_uri (EProxy* proxy, const gchar * uri);

gboolean
rss_ep_need_proxy_http (EProxy* proxy, const gchar * host, SoupAddress *addr);

gboolean
rss_ep_need_proxy_https (EProxy* proxy, const gchar * host);

gboolean
rss_ep_is_in_ignored (EProxy *proxy, const gchar *host);

#endif /*__RSS_EVO_COMMON_H_*/

