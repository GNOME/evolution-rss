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

#include <string.h>
#if (DATASERVER_VERSION >= 2031001)
#include <camel/camel.h>
#else
#include <camel/camel-data-cache.h>
#include <camel/camel-file-utils.h>
#endif

#include "rss.h"
#include "rss-cache.h"

#define CAMEL_DATA_CACHE_BITS (6)
#define CAMEL_DATA_CACHE_MASK ((1<<CAMEL_DATA_CACHE_BITS)-1)

#define HTTP_CACHE_PATH "http"

static CamelDataCache *cache = NULL;

void
rss_cache_init(void)
{
	//CamelDataCache *cache = NULL;
	gchar *base_dir, *feed_dir;

	base_dir = rss_component_peek_base_directory();
	feed_dir = g_build_path(G_DIR_SEPARATOR_S,
			base_dir,
			"static",
			NULL);
	g_free(base_dir);
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);
#if EVOLUTION_VERSION >= 23100
	cache = camel_data_cache_new(feed_dir, NULL);
#else
	cache = camel_data_cache_new(feed_dir, 0, NULL);
#endif
	g_free(feed_dir);

	if (!cache)
		return;

	// expire in a month max
	// and one week if not accessed sooner
	camel_data_cache_set_expire_age(cache, 24*60*60*30);
	camel_data_cache_set_expire_access(cache, 24*60*60*7);
}

char *
rss_cache_get_path(int create, const char *key)
{
	char *dir, *real;
	char *tmp = NULL;
	guint32 hash;

	hash = g_str_hash(key);
	hash = (hash>>5)&CAMEL_DATA_CACHE_MASK;
#if (DATASERVER_VERSION >= 2031001)
	dir = alloca(strlen(camel_data_cache_get_path(cache))
		+ strlen(HTTP_CACHE_PATH) + 8);
	sprintf(dir, "%s" G_DIR_SEPARATOR_S "%s" G_DIR_SEPARATOR_S "%02x",
		camel_data_cache_get_path(cache),
		HTTP_CACHE_PATH, hash);
#else
	dir = alloca(strlen(cache->path)
		+ strlen(HTTP_CACHE_PATH) + 8);
	sprintf(dir, "%s" G_DIR_SEPARATOR_S "%s" G_DIR_SEPARATOR_S "%02x", cache->path, HTTP_CACHE_PATH, hash);
#endif
	tmp = camel_file_util_safe_filename(key);
	if (!tmp)
		return NULL;
	real = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", dir, tmp);
	g_free(tmp);

	return real;
}

CamelStream*
rss_cache_get(gchar *url)
{
	return camel_data_cache_get(cache, HTTP_CACHE_PATH, url, NULL);
}

#if DATASERVER_VERSION <= 2025004
#define CAMEL_DATA_CACHE_BITS (6)
#define CAMEL_DATA_CACHE_MASK ((1<<CAMEL_DATA_CACHE_BITS)-1)

static char *
data_cache_path(
	CamelDataCache *cdc, int create, const char *path, const char *key)
{
	char *dir, *real;
	char *tmp = NULL;
	guint32 hash;

	hash = g_str_hash(key);
	hash = (hash>>5)&CAMEL_DATA_CACHE_MASK;
	dir = alloca(strlen(cdc->path) + strlen(path) + 8);
	sprintf(dir, "%s/%s/%02x", cdc->path, path, hash);
	tmp = camel_file_util_safe_filename(key);
	if (!tmp)
		return NULL;
	real = g_strdup_printf("%s/%s", dir, tmp);
	g_free(tmp);

	return real;
}
#endif


gchar*
rss_cache_get_filename(gchar *url)
{
#if DATASERVER_VERSION <= 2025004
	return data_cache_path(cache, FALSE, HTTP_CACHE_PATH, url);
#else
	return camel_data_cache_get_filename(cache, HTTP_CACHE_PATH, url, NULL);
#endif
}

CamelStream*
rss_cache_add(gchar *url)
{
	return camel_data_cache_add(cache, HTTP_CACHE_PATH, url, NULL);
}

