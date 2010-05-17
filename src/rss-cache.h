/* Evoution RSS Reader Plugin
 * Copyright (C) 2007-2010 Lucian Langa <cooly@gnome.eu.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#if (DATASERVER_VERSION >= 2031001)
#include <camel/camel.h>
#else
#include <camel/camel-data-cache.h>
#endif

void rss_cache_init(void);
CamelStream *rss_cache_get(gchar *url);
gchar *rss_cache_get_filename(gchar *url);
char* rss_cache_get_path(int create, const char *path);
CamelStream* rss_cache_add(gchar *url);
