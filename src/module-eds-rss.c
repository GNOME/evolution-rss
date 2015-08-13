/*
 *  Copyright (C) 2015 Red Hat Inc. <www.redhat.com>
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
#include <config.h>
#endif

#include <gmodule.h>

#include <libedataserver/libedataserver.h>
#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_RSS_CACHE_REAPER_EXT \
	(e_cache_reaper_get_type ())
#define E_RSS_CACHE_REAPER_EXT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_RSS_CACHE_REAPER_EXT, ERSSCacheReaperExt))
#define E_IS_RSS_CACHE_REAPER_EXT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_RSS_CACHE_REAPER_EXT))

typedef struct _ERSSCacheReaperExt ERSSCacheReaperExt;
typedef struct _ERSSCacheReaperExtClass ERSSCacheReaperExtClass;

GType e_rss_cache_reaper_ext_get_type (void);

void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

struct _ERSSCacheReaperExt {
	EExtension parent;
};

struct _ERSSCacheReaperExtClass {
	EExtensionClass parent_class;
};

G_DEFINE_DYNAMIC_TYPE (ERSSCacheReaperExt, e_rss_cache_reaper_ext, E_TYPE_EXTENSION)

static void
e_rss_cache_reaper_ext_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	e_cache_reaper_add_private_directory (E_CACHE_REAPER (extensible), "rss");

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_rss_cache_reaper_ext_parent_class)->constructed (object);
}

static void
e_rss_cache_reaper_ext_class_init (ERSSCacheReaperExtClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = e_rss_cache_reaper_ext_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CACHE_REAPER;
}

static void
e_rss_cache_reaper_ext_class_finalize (ERSSCacheReaperExtClass *class)
{
}

static void
e_rss_cache_reaper_ext_init (ERSSCacheReaperExt *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	/* Register dynamically loaded types. */
	e_rss_cache_reaper_ext_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
