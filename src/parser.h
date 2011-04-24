/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2009 Lucian Langa <cooly@gnome.eu.org>
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __PARSER_H__
#define __PARSER_H__

#include <mail/em-format-html.h>

gchar *update_channel(RDF *r);
xmlDoc *rss_html_url_decode(const char *html, int len);
GString *rss_strip_html (gchar *string);

const char *layer_find (xmlNodePtr node,
			const char *match,
			const char *fail);

const char *layer_find_innerelement (xmlNodePtr node,
					const char *match,
					const char *el,
					const char *fail);

gchar *layer_find_innerhtml (xmlNodePtr node,
				const char *match,
				const char *submatch,
				gchar *fail);

xmlNodePtr layer_find_pos (xmlNodePtr node,
		const char *match,
		const char *submatch);

gchar *
layer_query_find_prop (xmlNodePtr node,
		const char *match,
		xmlChar *attr,
		const char *attrprop,
		xmlChar *prop);

const char *layer_find_tag (xmlNodePtr node,
			const char *match,
			const char *fail);

char *layer_find_url (xmlNodePtr node, char *match, char *fail);

GList*
layer_find_tag_prop (xmlNodePtr node,
				const char *match,
				const char *search);

const char *
layer_find_ns_tag (xmlNodePtr node,
			const char *nsmatch,
			const char *match,
			const char *fail);

gchar *encode_html_entities(gchar *source);
gchar *decode_entities(gchar *source);
GList *layer_find_all (xmlNodePtr node, const char *match, const char *fail);
xmlDoc *parse_html(char *url, const char *html, int len);
xmlDoc *parse_html_sux (const char *buf, guint len);
xmlDoc *xml_parse_sux (const char *buf, int len);
create_feed *parse_channel_line(xmlNode *top,
				gchar *feed_name,
				RDF *r,
				gchar **article_uid);
gchar *tree_walk (xmlNodePtr root, RDF *r);
xmlNode *html_find (xmlNode *node, gchar *match);
xmlNode *html_find_s (xmlNode *node, gchar **match);
void html_set_base(xmlNode *doc, char *base, const char *tag, const char *prop, char *basehref);
gchar *content_rss(xmlNode *node, gchar *fail);
gchar *media_rss(xmlNode *node, gchar *search, gchar *fail);
gchar *dublin_core_rss(xmlNode *node, gchar *fail);
void syndication_rss(void);
gchar *wfw_rss(xmlNode *node, gchar *fail);
gchar *process_images(gchar *text, gchar *link, gboolean decode, EMFormatHTML *format);

#endif /*__RSS_H__*/

