/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2008 Lucian Langa <cooly@gnome.eu.org> 
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

gchar *update_channel(RDF *r);

char *layer_find (xmlNodePtr node, char *match, char *fail);
char *layer_find_innerelement (xmlNodePtr node, char *match, char *el, char *fail);
gchar *layer_find_innerhtml (xmlNodePtr node, char *match, char *submatch, char *fail);
xmlNodePtr layer_find_pos (xmlNodePtr node, char *match, char *submatch);
char *layer_find_tag (xmlNodePtr node, char *match, char *fail);
char * layer_find_url (xmlNodePtr node, char *match, char *fail);
char *layer_find_tag_prop (xmlNodePtr node, char *match, char *search, char *fail);
gchar *encode_html_entities(gchar *source);
gchar *decode_entities(gchar *source);
GList *layer_find_all (xmlNodePtr node, char *match, char *fail);
xmlDoc *parse_html(char *url, const char *html, int len);
xmlDoc *parse_html_sux (const char *buf, guint len);
xmlDoc *xml_parse_sux (const char *buf, int len);
create_feed *parse_channel_line(xmlNode *top, gchar *feed_name, char *main_date);
gchar *tree_walk (xmlNodePtr root, RDF *r);

#endif /*__RSS_H__*/

