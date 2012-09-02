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

#include "e-mail-formatter-evolution-rss.h"

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <shell/e-shell-settings.h>
#include <shell/e-shell.h>

#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#include <glib/gi18n-lib.h>
#include <X11/Xlib.h>
#include <camel/camel.h>

#include "rss-formatter.h"


typedef struct _EMailFormatterRSS EMailFormatterRSS;
typedef struct _EMailFormatterRSSClass EMailFormatterRSSClass;

struct _EMailFormatterRSS {
	EExtension parent;
};

struct _EMailFormatterRSSClass {
	EExtensionClass parent_class;
};

GType e_mail_formatter_evolution_rss_get_type (void);
static void e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailFormatterRSS,
	e_mail_formatter_evolution_rss,
	E_TYPE_EXTENSION,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_formatter_extension_interface_init));

static const gchar* formatter_mime_types[] = { "application/vnd.evolution.attachment" , NULL };

static gboolean
emfe_evolution_rss_format (EMailFormatterExtension *extension,
				EMailFormatter *formatter,
				EMailFormatterContext *context,
				EMailPart *part,
				CamelStream *stream,
				GCancellable *cancellable)
{
	CamelStream *decoded_stream;
	CamelDataWrapper *dw;
	gchar *str;
	GByteArray *ba;
	gchar *src;

	CamelContentType *ct = camel_mime_part_get_content_type (part->part);
	if (ct) {
		if (!camel_content_type_is (ct, "x-evolution", "evolution-rss-feed"))
			return FALSE;
	}

	dw = camel_medium_get_content (CAMEL_MEDIUM (part->part));
	if (!dw) {
		return FALSE;
	}

	str = g_strdup_printf (
		"<div class=\"part-container\" style=\"border-color: #%06x; "
		"background-color: #%06x; color: #%06x;\">"
		"<div class=\"part-container-inner-margin\">\n",
		e_color_to_value ((GdkColor *)
			e_mail_formatter_get_color (formatter, E_MAIL_FORMATTER_COLOR_FRAME)),
		e_color_to_value ((GdkColor *)
			e_mail_formatter_get_color (formatter, E_MAIL_FORMATTER_COLOR_CONTENT)),
		e_color_to_value ((GdkColor *)
			e_mail_formatter_get_color (formatter, E_MAIL_FORMATTER_COLOR_TEXT)));

	camel_stream_write_string (
		stream, str, cancellable, NULL);

	decoded_stream = camel_stream_mem_new ();

	e_mail_formatter_format_text (
		formatter, part, decoded_stream, cancellable);

	g_seekable_seek (G_SEEKABLE (decoded_stream), 0, G_SEEK_SET, cancellable, NULL);

	ba = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (decoded_stream));
	src = rss_process_feed((gchar *)ba->data, ba->len);

	camel_stream_write_string(stream, src, cancellable, NULL);
	g_free(src);
	g_object_unref (decoded_stream);

	camel_stream_write_string (
		stream, "</div></div>", cancellable, NULL);

	return TRUE;
}

static const gchar *
emfe_evolution_rss_get_display_name (EMailFormatterExtension *extension)
{
	return _("Text Highlighting");
}

static const gchar *
emfe_evolution_rss_get_description (EMailFormatterExtension *extension)
{
	return _("Syntax highlighting of mail parts");
}

static const gchar **
emfe_evolution_rss_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
e_mail_formatter_evolution_rss_init (EMailFormatterRSS *object)
{
}

static void
e_mail_formatter_evolution_rss_constructed (GObject *object)
{
	EExtensible *extensible;
	EMailExtensionRegistry *reg;

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	reg = E_MAIL_EXTENSION_REGISTRY (extensible);

	e_mail_extension_registry_add_extension (reg, E_MAIL_EXTENSION (object));
}

static void
e_mail_formatter_evolution_rss_class_init (EMailFormatterRSSClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = e_mail_formatter_evolution_rss_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY;
}

static void
e_mail_formatter_evolution_rss_class_finalize (EMailFormatterRSSClass *class)
{
}

void
e_mail_formatter_evolution_rss_type_register (GTypeModule *type_module)
{
	e_mail_formatter_evolution_rss_register_type (type_module);
}

static void
e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emfe_evolution_rss_format;
	iface->get_display_name = emfe_evolution_rss_get_display_name;
	iface->get_description = emfe_evolution_rss_get_description;
}

static void
e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emfe_evolution_rss_mime_types;
}

