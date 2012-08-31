/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2012 Lucian Langa <cooly@gnome.eu.org>
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

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <camel/camel.h>

#include "e-mail-parser-evolution-rss.h"

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>
#include <em-format/e-mail-part-utils.h>

#include <libebackend/libebackend.h>


typedef struct _EMailParserRSS {
	EExtension parent;
} EMailParserRSS;

typedef struct _EMailParserRSSClass {
	EExtensionClass parent_class;
} EMailParserRSSClass;

GType e_mail_parser_evolution_rss_get_type (void);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);
static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailParserRSS,
	e_mail_parser_evolution_rss,
	E_TYPE_EXTENSION,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar* pser_mime_types[] = { "x-evolution/evolution-rss-feed", NULL };

static GSList *
empe_evolution_rss_parse (EMailParserExtension *extension,
				EMailParser *parser,
				CamelMimePart *part,
				GString *part_id,
				GCancellable *cancellable)
{
	GSList *parts = e_mail_parser_parse_part_as (
			parser, part, part_id, "application/vnd.evolution.attachment", cancellable);

	return parts;
}

static const gchar **
empe_rss_mime_types (EMailExtension *extension)
{
	return pser_mime_types;
}

void
e_mail_parser_evolution_rss_type_register (GTypeModule *type_module)
{
	e_mail_parser_evolution_rss_register_type (type_module);
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_rss_mime_types;
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_evolution_rss_parse;
}

static void
e_mail_parser_evolution_rss_constructed (GObject *object)
{
	EExtensible *extensible;
	EMailExtensionRegistry *reg;

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	reg = E_MAIL_EXTENSION_REGISTRY (extensible);

	e_mail_extension_registry_add_extension (reg, E_MAIL_EXTENSION (object));
}

static void
e_mail_parser_evolution_rss_class_init (EMailParserRSSClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = e_mail_parser_evolution_rss_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY;
}

void
e_mail_parser_evolution_rss_class_finalize (EMailParserRSSClass *class)
{
}

static void
e_mail_parser_evolution_rss_init (EMailParserRSS *parser)
{

}

