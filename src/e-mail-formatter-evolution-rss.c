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

#include <shell/e-shell.h>

#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

#include "misc.h"
#include "rss-formatter.h"
#include "e-mail-part-rss.h"

extern gchar *commstream;
extern int rss_init;

typedef EMailFormatterExtension EMailFormatterRSS;
typedef EMailFormatterExtensionClass EMailFormatterRSSClass;

GType e_mail_formatter_evolution_rss_get_type (void);


G_DEFINE_DYNAMIC_TYPE (
	EMailFormatterRSS,
	e_mail_formatter_evolution_rss,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar* rss_formatter_mime_types[] = { "x-evolution/evolution-rss-feed", NULL };

static void
set_view_cb (GtkWidget *button,
		gpointer *data)
{
	rss_set_current_view(rss_get_current_view()^1);
	rss_set_changed_view(1);
	e_mail_display_reload (rss_get_display());
}
#include "fetch.h"

typedef struct _HD HD;
struct _HD {
	gchar *website;
	gchar *content;
	gchar *current_html;
	EMailFormatter *formatter;
	gchar *header;
#if EVOLUTION_VERSION < 31191
	CamelStream *stream;
#else
	GOutputStream *stream;
#endif
};

static gboolean
feed_async(gpointer key)
{
	HD *hd = (HD *)key;
	gchar *result;
	e_mail_display_load_images(rss_get_display());
	e_web_view_load_string (E_WEB_VIEW (rss_get_display()), hd->content);

	return FALSE;
}

static gboolean
emfe_evolution_rss_format (EMailFormatterExtension *extension,
				EMailFormatter *formatter,
				EMailFormatterContext *context,
				EMailPart *part,
#if EVOLUTION_VERSION < 31191
				CamelStream *stream,
#else
				GOutputStream *stream,
#endif
				GCancellable *cancellable)
{
	CamelDataWrapper *dw;
	gchar *str;
	gchar *h;
#if EVOLUTION_VERSION < 31191
	CamelStream *decoded_stream;
	GByteArray *ba;
#else
	GOutputStream *decoded_stream;
	gpointer data;
	gsize len;
#endif
	gchar *src;
	CamelMimePart *message = e_mail_part_ref_mime_part (part);
	gchar *website, *subject, *category, *feedid, *comments;
	guint32 frame_col, cont_col, text_col;
	gboolean is_html = FALSE;
	gchar *feed_dir, *tmp_file, *tmp_path, *iconfile;
	GdkPixbuf *pixbuf;

	CamelContentType *ct = camel_mime_part_get_content_type (message);
	if (ct) {
		if (!camel_content_type_is (ct, "x-evolution", "evolution-rss-feed"))
			goto fail;
	}

	dw = camel_medium_get_content (CAMEL_MEDIUM (message));
	if (!dw) {
		goto fail;
	}

	if (!rss_init)
			goto fail;

	h = g_strdup(e_web_view_get_html (E_WEB_VIEW (rss_get_display())));

	website = (gchar *)camel_medium_get_header (
			CAMEL_MEDIUM (message), "Website");
	if (!website)
		website = (gchar *)camel_medium_get_header (
			CAMEL_MEDIUM (message), "X-evolution-rss-website");

	feedid  = (gchar *)camel_medium_get_header(
				CAMEL_MEDIUM(message), "RSS-ID");
	if (!feedid)
		feedid  = (gchar *)camel_medium_get_header(
				CAMEL_MEDIUM(message), "X-evolution-rss-RSS-ID");
	comments  = (gchar *)camel_medium_get_header (
				CAMEL_MEDIUM(message),
				"X-Evolution-rss-comments");
	if (comments)
		comments = g_strstrip(comments);
	category  = (gchar *)camel_medium_get_header(
				CAMEL_MEDIUM(message),
				"X-Evolution-rss-category");
	subject = camel_header_decode_string(
			camel_medium_get_header (CAMEL_MEDIUM (message),
			"Subject"), NULL);
	if (!subject)
		subject = camel_header_decode_string(
			camel_medium_get_header (CAMEL_MEDIUM (message),
			"X-evolution-rss-subject"), NULL);

	if (feedid)
		is_html = rss_get_is_html(feedid);

	if (!rss_get_changed_view())
		rss_set_current_view(is_html);
	else
		rss_set_changed_view(0);


	feed_dir = rss_component_peek_base_directory();
	tmp_file = g_strconcat(feedid, ".img", NULL);
	tmp_path = g_build_path(G_DIR_SEPARATOR_S,
			feed_dir, tmp_file, NULL);
	g_free(tmp_file);
	g_free(feed_dir);
	iconfile = g_strconcat("evo-file://", tmp_path, NULL);
	if (g_file_test(tmp_path, G_FILE_TEST_EXISTS)){
		if (!(pixbuf = gdk_pixbuf_new_from_file(tmp_path, NULL))) {
			tmp_file = g_build_filename (EVOLUTION_ICONDIR, "rss-16.png", NULL);
			iconfile = g_strconcat("evo-file://", tmp_file, NULL);
			g_free(tmp_file);
		}
	} else {
		tmp_file = g_build_filename (EVOLUTION_ICONDIR, "rss-16.png", NULL);
		iconfile = g_strconcat("evo-file://", tmp_file, NULL);
		g_free(tmp_file);
	}

	frame_col = e_rgba_to_value (
			e_mail_formatter_get_color (formatter, E_MAIL_FORMATTER_COLOR_FRAME));
	cont_col = e_rgba_to_value (
			e_mail_formatter_get_color (formatter, E_MAIL_FORMATTER_COLOR_CONTENT));
	text_col = e_rgba_to_value (
			e_mail_formatter_get_color (formatter, E_MAIL_FORMATTER_COLOR_TEXT));

	if (!is_html && !rss_get_current_view()) {
		str = g_strdup_printf (
			"<div class=\"part-container\" style=\"border-color: #%06x; "
			"background-color: #%06x; color: #%06x;\">"
			"<div class=\"part-container-inner-margin\">\n"
			"<div style=\"border: solid 0px; background-color: #%06x; padding: 0px; spacing: 1px; color: #%06x;\">"
			"&nbsp;<img height=13 src=%s>&nbsp;"
			"<b><font size=+1><a href=%s>%s</a></font></b></div>",
			frame_col,
			cont_col,
			text_col,
			cont_col & 0xEDECEB & 0xffffff,
			text_col & 0xffffff,
			iconfile, website, subject);
		if (category) {
			gchar *fstr;
			gchar *tmp = g_strdup_printf (
				"<div style=\"border: solid 0px; background-color: #%06x; padding: 2px; color: #%06x;\">"
				"<b><font size=-1>%s: %s</font></b></div>",
				cont_col & 0xEDECEB & 0xffffff,
				text_col & 0xffffff,
				_("Posted under"), category);
			fstr = g_strconcat(str, tmp, NULL);
			g_free (tmp);
			g_free (str);
			str = fstr;
		}

#if EVOLUTION_VERSION < 31191
		camel_stream_write_string (
			stream, str, cancellable, NULL);
#else
		g_output_stream_write_all(stream, str,
			strlen(str), NULL, cancellable, NULL);
#endif

#if EVOLUTION_VERSION < 31191
		decoded_stream = camel_stream_mem_new ();
#else
		decoded_stream = g_memory_output_stream_new_resizable ();
#endif

		e_mail_formatter_format_text (
			formatter, part, decoded_stream, cancellable);

#if EVOLUTION_VERSION < 31191
		g_seekable_seek (G_SEEKABLE (decoded_stream), 0, G_SEEK_SET, cancellable, NULL);

		ba = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (decoded_stream));
		src = rss_process_feed((gchar *)ba->data, ba->len);
#else
		data = g_memory_output_stream_get_data (
				G_MEMORY_OUTPUT_STREAM (decoded_stream));
		len = g_memory_output_stream_get_data_size (
				G_MEMORY_OUTPUT_STREAM (decoded_stream));
		src = rss_process_feed((gchar *)data, len);
#endif
#if EVOLUTION_VERSION < 30304
		GConfClient *client = gconf_client_get_default();
#else
		GSettings *rss_settings = g_settings_new(RSS_CONF_SCHEMA);
#endif
#if EVOLUTION_VERSION < 30304
		if (comments && gconf_client_get_bool (client, GCONF_KEY_SHOW_COMMENTS, NULL)) {
#else
		if (comments && g_settings_get_boolean (rss_settings, CONF_SHOW_COMMENTS)) {
#endif
			if (commstream) {
				gchar *result = print_comments(comments, commstream, formatter);
				g_free(commstream);
				if (result && strlen(result)) {
					gchar *fstr;
					gchar *tmp = g_strdup_printf (
						"<div style=\"border: solid #%06x 0px; background-color: #%06x; padding: 10px; color: #%06x;\">%s</div>",
						frame_col & 0xffffff,
						cont_col & 0xffffff,
						text_col & 0xffffff,
						result);
					g_free(result);
					fstr = g_strconcat(src, tmp, NULL);
					g_free (tmp);
					g_free (src);
					src = fstr;
				}
				commstream = NULL;
			} else {
				gchar *uri = get_feed_url_by_feed_id(feedid);
				fetch_comments(comments, g_strdup(uri), rss_get_display());
			}
		}

#if EVOLUTION_VERSION < 31191
		camel_stream_write_string(stream, src, cancellable, NULL);
#else
		g_output_stream_write_all(stream, src, strlen(src), NULL, cancellable, NULL);
#endif
		g_free(src);
		g_object_unref (decoded_stream);

#if EVOLUTION_VERSION < 31191
		camel_stream_write_string (
			stream, "</div></div>", cancellable, NULL);
#else
		{
		gchar *tstr = g_strdup("</div></div>");
		g_output_stream_write_all(stream, tstr,
				strlen(tstr), NULL, cancellable, NULL);
		g_free(tstr);
		}
#endif
	} else {
		GString *content;
		GError *err = NULL;
		gchar *str;
		HD *hd = g_malloc0(sizeof(*hd));
		hd->current_html = h;
		hd->formatter = formatter;
		hd->header = e_mail_formatter_get_html_header(formatter);
		hd->stream = stream;
		content = fetch_blocking(website, NULL, NULL, textcb, NULL, &err);
		if (err) {
			//we do not need to setup a pop error menu since we're in
			//formatting process. But instead display mail body an error
			//such proxy error or transport error
			str = g_strdup_printf (
				"<div style=\"border: solid #%06x 1px; background-color: #%06x; color: #%06x;\">\n",
				frame_col & 0xffffff,
				cont_col & 0xffffff,
				text_col & 0xffffff);
#if EVOLUTION_VERSION < 31191
			camel_stream_write_string (stream, str, cancellable, NULL);
			camel_stream_write_string (stream, "<div style=\"border: solid 0px; padding: 4px;\">\n", cancellable, NULL);
			camel_stream_write_string (stream, "<h3>Error!</h3>", cancellable, NULL);
			camel_stream_write_string (stream, err->message, cancellable, NULL);
			camel_stream_write_string (stream, "</div>", cancellable, NULL);
#else
			{
			gchar *tstr;
			g_output_stream_write_all(stream, str, strlen(str), NULL, cancellable, NULL);
			tstr = g_strdup("<div style=\"border: solid 0px; padding: 4px;\">\n");
			g_output_stream_write_all(stream, tstr, strlen(tstr), NULL, cancellable, NULL);
			g_free(tstr);
			tstr = g_strdup("<h3>Error!</h3>");
			g_output_stream_write_all(stream, tstr, strlen(tstr), NULL, cancellable, NULL);
			g_free(tstr);
			g_output_stream_write_all(stream, err->message, strlen(err->message), NULL, cancellable, NULL);
			tstr = g_strdup("</div>");
			g_output_stream_write_all(stream, tstr, strlen(tstr), NULL, cancellable, NULL);
			g_free(tstr);
			}
#endif
			g_free (str);
			goto success;
		}

		hd->content = rss_process_website(content->str, website);
		hd->website = website;

		g_idle_add((GSourceFunc)feed_async, hd);
	}

success:g_object_unref(message);
	return TRUE;
fail:
	g_object_unref(message);
	return FALSE;
}

static void
e_mail_formatter_evolution_rss_init (EMailFormatterExtension *object)
{
}

void
e_mail_formatter_evolution_rss_type_register (GTypeModule *type_module)
{
	e_mail_formatter_evolution_rss_register_type (type_module);
}

static GtkWidget *
emfe_evolution_rss_get_widget (EMailFormatterExtension *extension,
				EMailPartList *context,
				EMailPart *part,
				GHashTable *params)
{
	GtkWidget *box, *button;
	box = gtk_hbutton_box_new ();

	button = gtk_button_new_with_label (rss_get_current_view() ? _("Show Summary") :
							_("Show Full Text"));
	g_signal_connect (button, "clicked", G_CALLBACK (set_view_cb), NULL);

	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
	button = gtk_button_new_with_label (rss_get_current_view() ? _("Show Summary") :
							_("Show Full Text"));
	g_signal_connect (button, "clicked", G_CALLBACK (set_view_cb), NULL);

	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
	gtk_widget_show(box);
	return box;
}

static void
e_mail_formatter_evolution_rss_class_init (EMailFormatterExtensionClass *class)
{
	class->mime_types = rss_formatter_mime_types;
	class->format = emfe_evolution_rss_format;
	class->get_widget = emfe_evolution_rss_get_widget;
	class->display_name = _("Evolution-RSS");
	class->description = _("Displaying RSS feed articles");
}

static void
e_mail_formatter_evolution_rss_class_finalize (EMailFormatterRSSClass *class)
{
}

