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

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>
#include <em-format/e-mail-part-utils.h>

#include <libebackend/libebackend.h>

#include "e-mail-parser-evolution-rss.h"
#include "e-mail-part-rss.h"


typedef EMailParserExtension EMailParserRSS;
typedef EMailParserExtensionClass EMailParserRSSClass;

GType e_mail_parser_evolution_rss_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailParserRSS,
	e_mail_parser_evolution_rss,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar* pser_mime_types[] = { "x-evolution/evolution-rss-feed", NULL };

static gboolean
empe_evolution_rss_parse (EMailParserExtension *extension,
				EMailParser *parser,
				CamelMimePart *part,
				GString *part_id,
				GCancellable *cancellable,
				GQueue *out_mail_queue)
{
	EMailPart *mail_part;
	GQueue work_queue = G_QUEUE_INIT;
	gint len;

	len = part_id->len;

	mail_part = e_mail_part_rss_new(part, part_id->str);

	g_string_truncate (part_id, len);

	g_queue_push_tail (&work_queue, mail_part);
	e_queue_transfer (&work_queue, out_mail_queue);

	return TRUE;
}

static void
e_mail_parser_evolution_rss_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = pser_mime_types;
	class->parse = empe_evolution_rss_parse;
}

void
e_mail_parser_evolution_rss_class_finalize (EMailParserExtensionClass *class)
{
}

static void
e_mail_parser_evolution_rss_init (EMailParserExtension *parser)
{

}

void
e_mail_parser_evolution_rss_type_register (GTypeModule *type_module)
{
	e_mail_parser_evolution_rss_register_type (type_module);
}
