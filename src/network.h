/* Evolution RSS Reader Plugin
 * Copyright (C) 2007-2009 Lucian Langa <cooly@gnome.eu.org>
 *
 * This progronam is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distopen_ributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICUrLAR PURPOSE.  See the
 * GNU General Public License for more dentails.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __NETWORK_H__
#define __NETWORK_H__

#include <glib.h>

#define GCONF_KEY_USE_PROXY "/apps/evolution/evolution-rss/use_proxy"
#define GCONF_KEY_HOST_PROXY "/apps/evolution/evolution-rss/host_proxy"
#define GCONF_KEY_PORT_PROXY "/apps/evolution/evolution-rss/port_proxy"
#define GCONF_KEY_AUTH_PROXY "/apps/evolution/evolution-rss/auth_proxy"
#define GCONF_KEY_USER_PROXY "/apps/evolution/evolution-rss/user_proxy"
#define GCONF_KEY_PASS_PROXY "/apps/evolution/evolution-rss/pass_proxy"
#define GCONF_KEY_NETWORK_TIMEOUT "/apps/evolution/evolution-rss/network_timeout"
#define GCONF_KEY_DOWNLOAD_QUEUE_SIZE "/apps/evolution/evolution-rss/network_queue_size"

/* GConf paths and keys from e-d-s e-proxy.c*/
#define PATH_GCONF_EVO_NETWORK_CONFIG "/apps/evolution/shell/network_config"
#define KEY_GCONF_EVO_PROXY_TYPE       PATH_GCONF_EVO_NETWORK_CONFIG "/proxy_type"

#define KEY_GCONF_EVO_USE_HTTP_PROXY    PATH_GCONF_EVO_NETWORK_CONFIG "/use_http_proxy"
#define KEY_GCONF_EVO_HTTP_HOST         PATH_GCONF_EVO_NETWORK_CONFIG "/http_host"
#define KEY_GCONF_EVO_HTTP_PORT         PATH_GCONF_EVO_NETWORK_CONFIG "/http_port"
#define KEY_GCONF_EVO_HTTP_USE_AUTH     PATH_GCONF_EVO_NETWORK_CONFIG "/use_authentication"
#define KEY_GCONF_EVO_HTTP_AUTH_USER    PATH_GCONF_EVO_NETWORK_CONFIG "/authentication_user"
#define KEY_GCONF_EVO_HTTP_AUTH_PWD     PATH_GCONF_EVO_NETWORK_CONFIG "/authentication_password"
#define KEY_GCONF_EVO_HTTP_IGNORE_HOSTS PATH_GCONF_EVO_NETWORK_CONFIG "/ignore_hosts"
#define KEY_GCONF_EVO_HTTPS_HOST        PATH_GCONF_EVO_NETWORK_CONFIG "/secure_host"
#define KEY_GCONF_EVO_HTTPS_PORT        PATH_GCONF_EVO_NETWORK_CONFIG "/secure_port"
#define KEY_GCONF_EVO_SOCKS_HOST        PATH_GCONF_EVO_NETWORK_CONFIG "/socks_host"
#define KEY_GCONF_EVO_SOCKS_PORT        PATH_GCONF_EVO_NETWORK_CONFIG "/socks_port"
#define KEY_GCONF_EVO_AUTOCONFIG_URL    PATH_GCONF_EVO_NETWORK_CONFIG "/autoconfig_url"

#define PATH_GCONF_SYS_PROXY "/system/proxy"
#define PATH_GCONF_SYS_HTTP_PROXY "/system/http_proxy"

#define KEY_GCONF_SYS_USE_HTTP_PROXY    PATH_GCONF_SYS_HTTP_PROXY "/use_http_proxy"
#define KEY_GCONF_SYS_HTTP_HOST         PATH_GCONF_SYS_HTTP_PROXY "/host"
#define KEY_GCONF_SYS_HTTP_PORT         PATH_GCONF_SYS_HTTP_PROXY "/port"
#define KEY_GCONF_SYS_HTTP_USE_AUTH     PATH_GCONF_SYS_HTTP_PROXY "/use_authentication"
#define KEY_GCONF_SYS_HTTP_AUTH_USER    PATH_GCONF_SYS_HTTP_PROXY "/authentication_user"
#define KEY_GCONF_SYS_HTTP_AUTH_PWD     PATH_GCONF_SYS_HTTP_PROXY "/authentication_password"
#define KEY_GCONF_SYS_HTTP_IGNORE_HOSTS PATH_GCONF_SYS_HTTP_PROXY "/ignore_hosts"
#define KEY_GCONF_SYS_HTTPS_HOST        PATH_GCONF_SYS_PROXY "/secure_host"
#define KEY_GCONF_SYS_HTTPS_PORT        PATH_GCONF_SYS_PROXY "/secure_port"
#define KEY_GCONF_SYS_SOCKS_HOST        PATH_GCONF_SYS_PROXY "/socks_host"
#define KEY_GCONF_SYS_SOCKS_PORT        PATH_GCONF_SYS_PROXY "/socks_port"
#define KEY_GCONF_SYS_AUTOCONFIG_URL    PATH_GCONF_SYS_PROXY "/autoconfig_url"

#define RIGHT_KEY(sufix) (proxy_type == PROXY_TYPE_SYSTEM ? KEY_GCONF_SYS_ ## sufix : KEY_GCONF_EVO_ ## sufix)


/*#define GCONF_E_SHELL_NETWORK_CONFIG_PATH "/apps/evolution/shell/network_config/"

#define GCONF_E_HTTP_HOST_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "http_host"
#define GCONF_E_HTTP_PORT_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "http_port"
#define GCONF_E_HTTPS_HOST_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "secure_host"
#define GCONF_E_HTTPS_PORT_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "secure_port"
#define GCONF_E_SOCKS_HOST_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "socks_host"
#define GCONF_E_SOCKS_PORT_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "socks_port"
#define GCONF_E_IGNORE_HOSTS_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "ignore_hosts"
#define GCONF_E_USE_AUTH_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "use_authentication"
#define GCONF_E_PROXY_TYPE_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "proxy_type"
#define GCONF_E_AUTH_USER_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "authentication_user"
#define GCONF_E_AUTH_PWD_KEY  GCONF_E_SHELL_NETWORK_CONFIG_PATH "authentication_password"
#define GCONF_E_USE_PROXY_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "use_http_proxy"
#define GCONF_E_AUTOCONFIG_URL_KEY GCONF_E_SHELL_NETWORK_CONFIG_PATH "autoconfig_url"*/

enum ProxyType {
        PROXY_TYPE_SYSTEM = 0,
        PROXY_TYPE_NO_PROXY,
        PROXY_TYPE_MANUAL,
        PROXY_TYPE_AUTO_URL /* no auto-proxy at the moment */
};

typedef enum {
	NET_ERROR_GENERIC,
	NET_ERROR_PROTOCOL,
	NET_ERROR_CANCELLED
} NetErrorCode;

typedef enum {
    NET_STATUS_NULL,
    NET_STATUS_BEGIN,
    NET_STATUS_SUCCESS,
    NET_STATUS_ERROR,
    NET_STATUS_PROGRESS,
    NET_STATUS_DONE
} NetStatusType;

typedef struct {
	guint32 current;
	guint32 total;
	gchar *chunk;
	guint chunksize;
	gboolean reset;         //signal to reset stream (usually because of redirect)
} NetStatusProgress;

typedef void (*NetStatusCallback)(NetStatusType status,
                                  gpointer statusdata,
                                  gpointer data);


#endif /* __NETWORK_H__ */
