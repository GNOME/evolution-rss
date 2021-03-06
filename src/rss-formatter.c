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

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libxml/HTMLparser.h>

#include <mail/e-mail-reader-utils.h>

#include "rss.h"
#include "parser.h"
#include "rss-image.h"

#include "rss-formatter.h"

extern int rss_verbose_debug;
extern rssfeed *rf;

gchar *
rss_process_feed(gchar *feed, guint len)
{
	xmlChar *buff = NULL;
	xmlDoc *src;
	xmlChar *wid;
	GdkPixbuf *pix;
	guint width;
	gchar *wids;
	int size;
	gchar *result;
	EMailReader *reader;
	EShellContent *shell_content;
	EShellView *shell_view;
	EMailDisplay *display;
	GtkAllocation alloc;

	shell_view = rss_get_mail_shell_view (TRUE);
	shell_content = e_shell_view_get_shell_content (shell_view);
	reader = E_MAIL_READER (shell_content);
	display = e_mail_reader_get_mail_display (reader);
	gtk_widget_get_allocation((GtkWidget *)display, &alloc);
	width = alloc.width - 56;
	wids = g_strdup_printf("%d", width);
	src = (xmlDoc *)parse_html_sux(feed, len);
	if (src) {
		xmlNode *doc = (xmlNode *)src;
		while ((doc = html_find(doc, (gchar *)"img"))) {
			int real_width = 0;
			GSettings *settings;
			xmlChar *url = xmlGetProp(doc, (xmlChar *)"src");
			gchar *real_image = verify_image(
						(gchar *)url,
						display);
			if (real_image) {
				xmlSetProp(
					doc,
					(xmlChar *)"src",
					(xmlChar *)real_image);
			}
			settings = g_settings_new(RSS_CONF_SCHEMA);
			if (g_settings_get_boolean (settings,
				CONF_IMAGE_RESIZE) && real_image) {
				pix = gdk_pixbuf_new_from_file(
					(const char *)real_image+7, //skip scheme part
					(GError **)NULL);
				if (pix)
					real_width = gdk_pixbuf_get_width(pix);

				d("real_image:%s\n", real_image);
				d("width:%d\n", width);
				d("real_width:%d\n", real_width);

				wid = xmlGetProp(
					doc,
					(xmlChar *)"width");
				if (wid) {
					if (atof((const char *)wid) > width)
						xmlSetProp(doc,
							(xmlChar *)"width",
							(xmlChar *)wids);
					g_free(wid);
					goto pixdone;
				} else if (real_width > width) {
					xmlSetProp(doc,
						(xmlChar *)"width",
						(xmlChar *)wids);
				}
pixdone:                        g_free(real_image);
			}
		}
		xmlDocDumpMemory(src, &buff, (int*)&size);
		xmlFree(src);
	}
	g_free(wids);
	result = g_strdup((gchar *)buff);
	xmlFree(buff);
	return result;
}

#include <libxml/HTMLtree.h>
#include <string.h>

gchar *
rss_process_website(gchar *content, gchar *website)
{
	gchar *tmp = decode_utf8_entities(content);
	xmlDoc *src = (xmlDoc *)parse_html(website, tmp, strlen(tmp));
	xmlChar *buff = NULL;
	int size;

	if (src) {
		htmlDocDumpMemory(src, &buff, &size);
		d("htmlDocDumpMemory:%s\n", buff);
		xmlFree(src);
		return (gchar *)buff;
	}
	return NULL;
}

gboolean
rss_get_current_view(void)
{
	return rf->cur_format;
}

void
rss_set_current_view(gboolean value)
{
	rf->cur_format = value;
}

gboolean
rss_get_changed_view(void)
{
	return rf->chg_format;
}

void
rss_set_changed_view(gboolean value)
{
	rf->chg_format = value;
}

gboolean
rss_get_is_html(gchar *feedid)
{
	gpointer ret;
	ret = g_hash_table_lookup(rf->hrh, feedid); //feedid is modified
	return ret ? TRUE:FALSE;
}

EMailDisplay *
rss_get_display(void)
{
	EMailReader *reader;
	EShellContent *shell_content;
	EShellView *shell_view;

	shell_view = rss_get_mail_shell_view (TRUE);
	shell_content = e_shell_view_get_shell_content (shell_view);
	reader = E_MAIL_READER (shell_content);
	return e_mail_reader_get_mail_display (reader);
}
