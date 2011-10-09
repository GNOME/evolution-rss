/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2010 Lucian Langa <cooly@gnome.eu.org>
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
#include <glib/gi18n-lib.h>

#include <libxml/parserInternals.h>
#include <libxml/xmlmemory.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/debugXML.h>
#include <mail/mail-ops.h>
#include <mail/em-format-html.h>
#if (DATASERVER_VERSION >= 2031001)
#include <camel/camel.h>
#else
#include <camel/camel-url.h>
#endif
#include <libedataserver/e-data-server-util.h>

extern int rss_verbose_debug;

#include "fetch.h"
#include "rss.h"
#include "rss-config.h"
#include "rss-image.h"
#include "rss-status-icon.h"
#include "parser.h"
#include "misc.h"
#include "network-soup.h"

extern GConfClient *rss_gconf;
void asyncr_context_free(AsyncData *asyncr);
GQueue *display_channel_items_sync(AsyncData *ayncr);

/************ RDF Parser *******************/

guint rsserror = FALSE;
extern rssfeed *rf;
extern gboolean feed_new;

//
// decodes url_encoded strings that might appear in a html body
//
xmlDoc *
rss_html_url_decode(const char *html, int len)
{
	xmlDoc *src = NULL;
	xmlDoc *doc = NULL;
	gchar *url;
	gchar *duri = NULL;

	src = (xmlDoc *)parse_html_sux(html, len);

	if (!src)
		return NULL;

	doc = src;

	while ((doc = (xmlDoc *)html_find((xmlNode *)doc, (gchar *)"img"))) {
		if ((url = (gchar *)xmlGetProp((xmlNodePtr)doc, (xmlChar *)"src"))) {
			if (strstr(url, "img:")) {
				gchar *tmp = decode_image_cache_filename(url);
				duri = g_strconcat("file://", tmp, NULL);
				g_free(tmp);
				xmlSetProp(
					(xmlNodePtr)doc,
					(xmlChar *)"src",
					(xmlChar *)duri);
			}
			xmlFree(url);
		}
	}
	return src;
}

void
html_set_base(
	xmlNode *doc, char *base, const char *tag,
	const char *prop, char *basehref)
{
	gchar *url;
#if LIBSOUP_VERSION < 2003000
	SoupUri *newuri;
#else
	SoupURI *newuri;
#endif
	gchar *newuristr;
#if LIBSOUP_VERSION < 2003000
	SoupUri *base_uri = soup_uri_new (base);
#else
	SoupURI *base_uri = soup_uri_new (base);
#endif
	while ((doc = html_find((xmlNode *)doc, (gchar *)tag))) {
		if ((url = (gchar *)xmlGetProp(doc, (xmlChar *)prop))) {
			if (!strncmp(tag, "img", 3) && !strncmp(prop, "src", 3)) {
				gchar *tmpurl = strplchr(url);
				xmlSetProp(doc, (xmlChar *)prop, (xmlChar *)tmpurl);
				g_free(tmpurl);
			}
			d("DEBUG: parsing: %s\n", url);
			if (url[0] == '/' && url[1] != '/') {
				gchar *server = get_server_from_uri(base);
				gchar *tmp = g_strdup_printf("%s/%s", server, url);
				xmlSetProp(doc, (xmlChar *)prop, (xmlChar *)tmp);
				g_free(tmp);
				g_free(server);
			}
			if (url[0] == '/' && url[1] == '/') {
				/*FIXME handle ssl */
				gchar *tmp = g_strdup_printf("%s%s", "http:", url);
				xmlSetProp(doc, (xmlChar *)prop, (xmlChar *)tmp);
				g_free(tmp);
			}
			if (url[0] != '/' && !g_str_has_prefix(url,  "http://")
					&& !g_str_has_prefix(url, "https://")) {
				// in case we have a base href= set then rewrite
				// all relative links
				if (basehref != NULL) {
#if LIBSOUP_VERSION < 2003000
					SoupUri *newbase_uri = soup_uri_new (basehref);
#else
					SoupURI *newbase_uri = soup_uri_new (basehref);
#endif
					newuri = soup_uri_new_with_base (newbase_uri, url);
					soup_uri_free(newbase_uri);
				} else
					newuri = soup_uri_new_with_base (base_uri, url);
				//xmlSetProp(doc, prop, g_strdup_printf("%s/%s", get_server_from_uri(base), url));
				if (newuri) {
					newuristr = soup_uri_to_string (newuri, FALSE);
					xmlSetProp(doc, (xmlChar *)prop, (xmlChar *)newuristr);
					g_free(newuristr);
					soup_uri_free(newuri);
				}
			}
			xmlFree(url);
		}
	}
	soup_uri_free(base_uri);
}

static void
my_xml_perror_handler (void *ctx, const char *msg, ...)
{
	rsserror = TRUE;
	g_print("xml_parse_sux(): ERROR:%s\n", msg);
}

static void
my_xml_parser_error_handler (void *ctx, const char *msg, ...)
{
	;
}

xmlDoc *
xml_parse_sux (const char *buf, int len)
{
	static xmlSAXHandler *sax;
	xmlParserCtxtPtr ctxt;
	xmlDoc *doc = NULL;
	gchar *mime_type;
	gboolean uncertain;

	rsserror = FALSE;

	g_return_val_if_fail (buf != NULL, NULL);
	/* we might be forced to lower detection buffer size as
	 * application/ is often misdetected as text/html
	 */
	mime_type = g_content_type_guess(NULL, (guchar *)buf, 100, &uncertain);
	d("mime:%s, uncertain:%d\n", mime_type, uncertain);
	//feeding parsed anything other than xml results in blocking delays
	//it's possible we can relax parser by using xmlErrorFunc
	//UPDATE: add text/* - but exclude text/html I've seen huge delays because of this
	//I doubt there'll be any text/html feeds
	if (!g_ascii_strncasecmp (mime_type, "application/", 12)
	   || (!g_ascii_strncasecmp (mime_type, "text/", 5)
	   && g_ascii_strncasecmp (mime_type, "text/html", 9))) {
		if (!sax) {
			xmlInitParser();
			sax = xmlMalloc (sizeof (xmlSAXHandler));
			xmlSAXVersion (sax, 2);
			sax->warning = my_xml_parser_error_handler;
			sax->error = my_xml_perror_handler;
		}

		if (len == -1)
			len = strlen (buf);
		ctxt = xmlCreateMemoryParserCtxt (buf, len);
		if (!ctxt)
			return NULL;

		xmlFree (ctxt->sax);
		ctxt->sax = sax;
		ctxt->sax2 = 1;
		ctxt->str_xml = xmlDictLookup (ctxt->dict, BAD_CAST "xml", 3);
		ctxt->str_xmlns = xmlDictLookup (ctxt->dict, BAD_CAST "xmlns", 5);
		ctxt->str_xml_ns = xmlDictLookup (ctxt->dict, XML_XML_NAMESPACE, 36);

		ctxt->recovery = TRUE;
		ctxt->vctxt.error = my_xml_parser_error_handler;
		ctxt->vctxt.warning = my_xml_parser_error_handler;


		xmlCtxtUseOptions(ctxt, XML_PARSE_DTDLOAD
				| XML_PARSE_NOENT);

		xmlParseDocument (ctxt);

		doc = ctxt->myDoc;
		ctxt->sax = NULL;
		xmlFreeParserCtxt (ctxt);
	} else {
		rsserror = TRUE;
	}
	g_free(mime_type);
	return doc;
}

xmlDoc *
parse_html_sux (const char *buf, guint len)
{
	xmlDoc *doc;
#if LIBXML_VERSION > 20600
	static xmlSAXHandler *sax;
	htmlParserCtxtPtr ctxt;

	g_return_val_if_fail (buf != NULL, NULL);

	if (!sax) {
		xmlInitParser();
		sax = xmlMalloc (sizeof (htmlSAXHandler));
		memcpy (sax, &htmlDefaultSAXHandler, sizeof (xmlSAXHandlerV1));
		sax->warning = my_xml_parser_error_handler;
		sax->error = my_xml_parser_error_handler;
	}

	if (len == -1)
		len = strlen (buf);
	ctxt = htmlCreateMemoryParserCtxt (buf, len);
	if (!ctxt)
		return NULL;

	xmlFree (ctxt->sax);
	ctxt->sax = sax;
	ctxt->vctxt.error = my_xml_parser_error_handler;
	ctxt->vctxt.warning = my_xml_parser_error_handler;

	htmlCtxtUseOptions(ctxt, HTML_PARSE_NONET
				| HTML_PARSE_COMPACT
				| HTML_PARSE_NOBLANKS);

	htmlParseDocument (ctxt);
	doc = ctxt->myDoc;

	ctxt->sax = NULL;
	htmlFreeParserCtxt (ctxt);

#else /* LIBXML_VERSION <= 20600 */
	char *buf_copy = g_strndup (buf, len);

	doc = htmlParseDoc (buf_copy, NULL);
	g_free (buf_copy);
#endif
	return doc;
}

/*modifies a html document to be absolute */
xmlDoc *
parse_html(char *url, const char *html, int len)
{
	xmlDoc *src = NULL;
	xmlDoc *doc = NULL;
	xmlDoc *tmpdoc;
	gchar *newbase = NULL;

	src = (xmlDoc *)parse_html_sux(html, len);

	if (!src)
		return NULL;
	doc = src;
	newbase = (gchar *)xmlGetProp(html_find((xmlNode *)doc, (gchar *)"base"), (xmlChar *)"href");
	d("newbase:|%s|\n", newbase);
	tmpdoc = (xmlDoc *)html_find((xmlNode *)doc, (gchar *)"base");
	xmlUnlinkNode((xmlNode *)tmpdoc);
	html_set_base((xmlNode *)doc, url, "a", "href", newbase);
	html_set_base((xmlNode *)doc, url, "img", "src", newbase);
	html_set_base((xmlNode *)doc, url, "input", "src", newbase);
	html_set_base((xmlNode *)doc, url, "link", "src", newbase);
	html_set_base((xmlNode *)doc, url, "body", "background", newbase);
	html_set_base((xmlNode *)doc, url, "script", "src", newbase);
/*      while (doc = html_find((xmlNode *)doc, "img"))
	{
		if (url = xmlGetProp(doc, "src"))
		{
			gchar *str = strplchr(url);
			g_print("%s\n", str);
			xmlSetProp(doc, "src", str);
			g_free(str);
			xmlFree(url);
		}
	}*/
	doc = src;
	if (newbase)
		xmlFree(newbase);
	return doc;
}

const char *
layer_find_innerelement (
	xmlNodePtr node,
	const char *match,
	const char *el,
	const char *fail)
{
	while (node!=NULL) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (strcasecmp ((char *)node->name, match)==0) {
			return (char *)xmlGetProp(node, (xmlChar *)el);
		}
		node = node->next;
	}
	return fail;
}

xmlNode *
html_find (xmlNode *node,
	gchar *match)
{
	while (node) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (node->children)
			node = node->children;
		else {
			while (node && !node->next)
				node = node->parent;
			if (!node)
				return NULL;
			node = node->next;
		}

		if (node->name) {
			if (!strcmp ((char *)node->name, match))
				return node;
		}
	}
	return NULL;
}

/* as html_find() but takes an array of strings for input */
xmlNode *
html_find_s (xmlNode *node,
	gchar **match)
{
	guint i;
	while (node) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (node->children)
			node = node->children;
		else {
			while (node && !node->next)
				node = node->parent;
			if (!node)
				return NULL;
			node = node->next;
		}

		if (node->name) {
			for (i = 0; match[i]; i++) {
				if (!g_strcmp0 ((char *)node->name, match[i]))
					return node;
			}
		}
	}
	return NULL;
}

/* returns node disregarding type
 */
const char *
layer_find (xmlNodePtr node,
	const char *match,
	const char *fail)
{
	while (node!=NULL) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (strcasecmp ((char *)node->name, match)==0) {
			if (node->children != NULL && node->children->content != NULL) {
				return (char *)(node->children->content);
			} else {
				return fail;
			}
		}
		node = node->next;
	}
	return fail;
}

/* returns all matched nodes disregarding type
 */

GList *
layer_find_all (xmlNodePtr node,
		const char *match,
		const char *fail)
{
	GList *category = NULL;
	while (node!=NULL) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (strcasecmp ((char *)node->name, match)==0) {
			while (node!=NULL && strcasecmp ((char *)node->name, match)==0) {
				if (node->children != NULL && node->children->content != NULL) {
					/* FIXME */
					category = g_list_append(category, g_strdup((char *)node->children->content));
				}
				node = node->next;
			}
		}
		if (node)
			node = node->next;
	}
	if (category)
		return category;
	else {
		g_list_free(category);
		return (GList *)fail;
	}
}

//
//namespace-based modularization
//standard modules
//
//	mod_content
//	* only handles content:encoding
//	* if it's necessary handle
//	  content:item stuff

gchar *
content_rss(xmlNode *node, gchar *fail)
{
	gchar *content;

	content = (char *)xmlNodeGetContent(node);
	if (content)
		return content;
	else
		return fail;
}

gchar *
dublin_core_rss(xmlNode *node, gchar *fail)
{
	gchar *content;

	content = (char *)xmlNodeGetContent(node);
	if (content)
		return content;
	else
		return fail;
}

void
syndication_rss(void)
{
	g_print("syndication\n");
}

gchar *
wfw_rss(xmlNode *node, gchar *fail)
{
	gchar *content;

	content = (char *)xmlNodeGetContent(node);
	if (content)
		return content;
	else
		return fail;
}

const gchar *standard_rss_modules[5][3] = {
	{"content", "content", (gchar *)content_rss},
	{"dublin core", "dc", (gchar *)dublin_core_rss},
	{"syndication", "sy", (gchar *)syndication_rss},
	{"well formed web", "wfw", (gchar *)wfw_rss},
	{"slashdot entities", "slash", (gchar *)dublin_core_rss}};

//<nsmatch:match>content</nsmatch:match>
const char*
layer_find_ns_tag(xmlNodePtr node,
		const char *nsmatch,
		const char *match,
		const char *fail)
{
	int i;
	char* (*func)();

	while (node!=NULL) {
		if (node->ns && node->ns->prefix) {
			for (i=0; i < 5; i++) {
				if (!strcasecmp ((char *)node->ns->prefix, standard_rss_modules[i][1])) {
					func = (gpointer)standard_rss_modules[i][2];
					if (strcasecmp ((char *)node->ns->prefix, nsmatch) == 0
					&&  strcasecmp ((char *)node->name, match) == 0 ) {
						return func(node, fail);
					}
				}
			}
		}
		node = node->next;
	}
	return fail;
}

/* find matching tag (with html entities) */
const char *
layer_find_tag (xmlNodePtr node,
		const char *match,
		const char *fail)
{
	xmlBufferPtr buf = xmlBufferCreate();
	gchar *content;
	int i;
	char* (*func)();

	while (node!=NULL) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (node->ns && node->ns->prefix) {
			for (i=0; i < 4; i++) {
				if (!strcasecmp ((char *)node->ns->prefix, standard_rss_modules[i][1])) {
					func = (gpointer)standard_rss_modules[i][2];
					if (strcasecmp ((char *)node->ns->prefix, match)==0) {
						xmlBufferFree(buf);
						return func(node, fail);
					}
				}
			}
		} else {		//in case was not a standard module process node normally
					//above case should handle all modules
			if (strcasecmp ((char *)node->name, match)==0) {
				if (node->type == 1) {			//XML_NODE_ELEMENT
					gchar *nodetype = (gchar *)xmlGetProp(node, (xmlChar *)"type");
					//we need separate xhtml parsing because of xmlNodegetcontent substitutes html entities
					if (nodetype && !strcasecmp(nodetype, "xhtml")) {		// test this with "html" or smth else
						//this looses html entities
						xmlNodeDump(buf, node->doc, node, 0, 0);
						content = g_strdup_printf("%s", xmlBufferContent(buf));
					} else {
						content = (char *)xmlNodeGetContent(node);
					}
					xmlBufferFree(buf);
					if (nodetype)
						xmlFree(nodetype);
					return content;
				} else {
					xmlBufferFree(buf);
					return fail;
				}
			}
		}
		node = node->next;
	}
	xmlBufferFree(buf);
	return fail;
}

gchar*
media_rss(xmlNode *node, gchar *search, gchar *fail)
{
	gchar *content;
	d("media_rss()\n");

	content = (gchar *)xmlGetProp(node, (xmlChar *)search);
	if (content)
		return content;
	else
		return fail;
}

const gchar *property_rss_modules[1][3] = {
	{"media", "media", (gchar *)media_rss}};

GList *
layer_find_tag_prop (xmlNodePtr node,
	const char *match,
	const char *search)
{
	int i = 0;
	char* (*func)();
	gchar *tmp;
	GList *result = NULL;

	while (node!=NULL) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (node->ns && node->ns->prefix) {
				if (!strcasecmp ((char *)node->ns->prefix, property_rss_modules[i][1])) {
					func = (gpointer)property_rss_modules[i][2];
					if (strcasecmp ((char *)node->ns->prefix, match)==0) {
						tmp =  func(node, search, NULL);
						if (tmp)
							result = g_list_append(result, tmp);
					}
				}
		}
		node = node->next;
	}
	return result;
}

gchar *
layer_find_innerhtml (xmlNodePtr node,
		const char *match, const char *submatch,
		gchar *fail)
{
	gchar *tmp;
	xmlNodePtr tmpnode;
	while (node!=NULL) {
		if (strcasecmp ((char *)node->name, match)==0 && node->children) {
			tmpnode = node->children;
			while (tmpnode && tmpnode->name) {
				if ((tmp = (gchar *)layer_find(tmpnode, submatch, NULL)))
					return tmp;
				tmpnode = tmpnode->next;
			}
		}
		node = node->next;
	}
	return fail;
}

xmlNodePtr
layer_find_pos (xmlNodePtr node,
		const char *match,
		const char *submatch)
{
	xmlNodePtr subnode;
	while (node!=NULL) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (strcasecmp ((char *)node->name, match)==0 && node->children) {
			subnode = node->children;
			while (subnode!=NULL) {
				if (subnode->name
				&& strcasecmp ((char *)subnode->name, submatch) == 0
				&& subnode->children) {
						return subnode->children->next;
				}
				subnode = subnode->next;
			}
		}
		node = node->next;
	}
	return NULL;
}

/*
 * locates a property using a query in the format atribute=property
 */

gchar *
layer_query_find_prop (xmlNodePtr node,
		const char *match,
		xmlChar *attr,
		const char *attrprop,
		xmlChar *prop)
{
	while (node!=NULL) {
		if (!g_ascii_strcasecmp((gchar *)node->name, match)) {
			gchar *tprop = (gchar *)xmlGetProp(node, attr);
			if (tprop) {
				if (g_ascii_strcasecmp(tprop, attrprop)) {
					xmlFree(tprop);
					goto next;
				}
			}
			xmlFree(tprop);
			return (gchar *)xmlGetProp(node, prop);
		}
next:		node = node->next;
	}
	return NULL;
}

/*
 * like layer_query_find_prop but locates all properties in an entry
 */

GList *
layer_query_find_all_prop (xmlNodePtr node,
		const char *match,
		xmlChar *attr,
		const char *attrprop,
		xmlChar *prop)
{
	gchar *tmp;
	GList *result = NULL;
	while (node!=NULL) {
		if (!g_ascii_strcasecmp((gchar *)node->name, match)) {
			gchar *tprop = (gchar *)xmlGetProp(node, attr);
			if (tprop) {
				if (g_ascii_strcasecmp(tprop, attrprop)) {
					xmlFree(tprop);
					goto next;
				}
			}
			xmlFree(tprop);
			tmp =  (gchar *)xmlGetProp(node, prop);
			if (tmp)
				result = g_list_append(result, tmp);
		}
next:		node = node->next;
	}
	return result;
}

char *
layer_find_url (xmlNodePtr node,
		char *match,
		char *fail)
{
	char *p = (char *)layer_find (node, match, fail);
	char *r = p;
	static char *wb = NULL;
	char *w;

	if (wb) {
		g_free (wb);
	}

	wb = w = g_malloc (3 * strlen (p));

	if (w == NULL) {
		return fail;
	}

	if (*r == ' ') r++;	/* Fix UF bug */

	while (*r) {
		if (strncmp (r, "&amp;", 5) == 0) {
			*w++ = '&';
			r += 5;
			continue;
		}
		if (strncmp (r, "&lt;", 4) == 0) {
			*w++ = '<';
			r += 4;
			continue;
		}
		if (strncmp (r, "&gt;", 4) == 0) {
			*w++ = '>';
			r += 4;
			continue;
		}
		if (*r == '"' || *r == ' '){
			*w++ = '%';
			*w++ = "0123456789ABCDEF"[*r/16];
			*w++ = "0123456789ABCDEF"[*r&15];
			r++;
			continue;
		}
		*w++ = *r++;
	}
	*w = 0;
	return wb;
}

gchar *
tree_walk (xmlNodePtr root, RDF *r)
{
	xmlNodePtr walk;
	xmlNodePtr rewalk = root;
	xmlNodePtr channel = NULL, image = NULL;
	GArray *item = g_array_new (TRUE, TRUE, sizeof (xmlNodePtr));
	gchar *t;
	gchar *md2, *tmp, *ver;

	do {
		walk = rewalk;
		rewalk = NULL;

		while (walk!=NULL){
#ifdef RDF_DEBUG
			printf ("%p, %s\n", walk, walk->name);
#endif
			if (strcasecmp ((char *)walk->name, "rdf") == 0) {
				rewalk = walk->children;
				walk = walk->next;
				if (!r->type)
					r->type = g_strdup("RDF");
				r->type_id = RDF_FEED;
				if (r->version)
					g_free(r->version);
				r->version = g_strdup("(RSS 1.0)");
				r->base = (gchar *)xmlGetProp(walk, (xmlChar *)"base");
				continue;
			}
			if (strcasecmp ((char *)walk->name, "rss") == 0){
				xmlNode *node = walk;
				rewalk = walk->children;
				walk = walk->next;
				if (!r->type)
				r->type = g_strdup("RSS");
				r->type_id = RSS_FEED;
				ver = (gchar *)xmlGetProp(node, (xmlChar *)"version");
				if (r->version)
					g_free(r->version);
				r->version = g_strdup(ver);
				if (ver)
					xmlFree(ver);
				r->base = (gchar *)xmlGetProp(node, (xmlChar *)"base");
				continue;
			}
			if (strcasecmp ((char *)walk->name, "feed") == 0) {
				xmlNode *node = walk;
				if (!r->type)
				r->type = g_strdup("ATOM");
				r->type_id = ATOM_FEED;
				ver = (gchar *)xmlGetProp(node, (xmlChar *)"version");
				if (ver) {
					if (r->version)
						g_free(r->version);
					r->version = g_strdup(ver);
					xmlFree(ver);
				} else {
					if (r->version)
						g_free(r->version);
					r->version = g_strdup("1.0");
				}
				r->base = (gchar *)xmlGetProp(walk, (xmlChar *)"base");
				if (!r->base)
					r->base = layer_query_find_prop (walk->children,
							"link",
							(xmlChar *)"rel",
							"alternate", (xmlChar *)"href");
			}

			/* This is the channel top level */
			d("Top level '%s'.\n", walk->name);
			if (strcasecmp ((char *)walk->name, "channel") == 0) {
				channel = walk;
				rewalk = channel->children;
			}
			if (strcasecmp ((char *)walk->name, "feed") == 0) {
				channel = walk;
				rewalk = channel->children;
			}
			if (strcasecmp ((char *)walk->name, "image") == 0) {
				image = walk;
			}
			if (strcasecmp ((char *)walk->name, "item") == 0) {
				g_array_append_val(item, walk);
			}
			if (strcasecmp ((char *)walk->name, "entry") == 0) {
				g_array_append_val(item, walk);
			}
			walk = walk->next;
		}
	} while (rewalk);

	if (channel == NULL) {
		fprintf(stderr, "ERROR:No channel definition.\n");
		return NULL;
	}

	if (image != NULL)
		r->image = (gchar *)layer_find(image->children, "url", NULL);

	t = g_strdup(get_real_channel_name(r->uri, NULL));
	if (t == NULL) {
		t = (gchar *)layer_find(channel->children,
				"title",
				g_strdup(DEFAULT_NO_CHANNEL));
		t = decode_html_entities(t);
		tmp = sanitize_folder(t);
		g_free(t);
		t = tmp;
		t = generate_safe_chn_name(t);
	}
	tmp = (gchar *)layer_find(channel->children, "ttl", NULL);
	if (tmp)
		r->ttl = atoi(tmp);
	else
		r->ttl = 0;

	//items might not have a date
	// so try to grab channel/feed date
	md2 = g_strdup(layer_find(channel->children, "date",
		layer_find(channel->children, "pubDate",
		layer_find(channel->children, "updated", NULL))));
	r->maindate = md2;
	r->total = item->len;
	r->item = item;
	r->title = t;
	return r->title;
}

static const char *html_const[4] = {"img", "a", NULL};

gchar *
process_images(gchar *text, gchar *link, gboolean decode, EMFormatHTML *format)
{
	xmlChar *buff = NULL;
	guint size = 0;
	gchar *tname;
	xmlDoc *src = (xmlDoc *)parse_html_sux (text, strlen(text));
	if (src) {
		xmlNode *doc = (xmlNode *)src;
		while ((doc = html_find_s(doc, (gchar **)html_const))) {
			gchar *name = NULL;
			xmlChar *url = xmlGetProp(doc, (xmlChar *)"src");
			if (url) {
				if ((name = fetch_image_redraw((gchar *)url, link, format))) {
					if (decode) {
						tname = decode_image_cache_filename(name);
#if (EVOLUTION_VERSION >= 23000)
						g_free(name);
						name = g_filename_to_uri (tname, NULL, NULL);
						g_free(tname);
#else
						name = tname;
#endif
					}
					xmlSetProp(
						doc, (xmlChar *)"src",
						(xmlChar *)name);
					g_free(name);
				}
				xmlFree(url);
			} else {
				gchar *url = (gchar *)xmlGetProp(doc, (xmlChar *)"href");
				if (url
				&& (g_ascii_strncasecmp (url, "http://", 7) &&
					g_ascii_strncasecmp (url, "https://", 8) &&
					g_ascii_strncasecmp (url, "ftp://", 6) &&
					g_ascii_strncasecmp (url, "nntp://", 7) &&
					g_ascii_strncasecmp (url, "mailto:", 7) &&
					g_ascii_strncasecmp (url, "news:", 5) &&
					g_ascii_strncasecmp (url, "file:", 5) &&
					g_ascii_strncasecmp (url, "callto:", 7) &&
					g_ascii_strncasecmp (url, "h323:", 5) &&
					g_ascii_strncasecmp (url, "sip:", 4) &&
					g_ascii_strncasecmp (url, "webcal:", 7))) {
					name = g_build_path(G_DIR_SEPARATOR_S, link, url, NULL);
					xmlFree(url);
					xmlSetProp(doc, (xmlChar *)"href",
						(xmlChar *)name);
					g_free(name);
				}
			}
		}
		xmlDocDumpMemory(src, &buff, (int*)&size);
		xmlFree(src);
		return (gchar *)buff;
	}
	return g_strdup(text);
}

static void
saxCharacters (void *user_data, const xmlChar *string, int len)
{
	GString *buffer = (GString *)user_data;
	g_string_append_len(buffer, (gchar *)string, len);
}

GString*
rss_strip_html (gchar *string)
{
	htmlParserCtxtPtr       ctxt;
	htmlSAXHandlerPtr       sax;
	GString *buffer = g_string_new(NULL);

	sax = g_new0 (htmlSAXHandler, 1);
	sax->characters = saxCharacters;
	ctxt = htmlCreatePushParserCtxt
		(sax, buffer, string, strlen (string), "", XML_CHAR_ENCODING_UTF8);
	htmlParseChunk (ctxt, string, 0, 1);
	htmlFreeParserCtxt (ctxt);
	g_free (sax);
	return buffer;
}

create_feed *
parse_channel_line(xmlNode *top, gchar *feed_name, RDF *r, gchar **article_uid)
{
	char *q = NULL;
	char *b = NULL;
	char *d2 = NULL;
	char *sp = NULL;
	char *d, *link = NULL;
	char *comments = NULL;
	char *p, *q1, *q2, *q3;
	gchar *feed = NULL;
	gchar *encl, *tmp, *id;
	gchar *qsafe, *tcat;
	GList *category = NULL;
	create_feed *CF;
	GList *attachments = NULL;
	gchar *base = NULL, *main_date = NULL;

	if (r) {
		base = r->base;
		main_date = r->maindate;
	}

	//we have to free this somehow
	//<link></link>
	link = g_strdup(layer_find (top, "link", NULL));		//RSS,
	if (!link) {                                                            // <link href=>
		if (!(link = layer_query_find_prop (top,
			"link",(xmlChar *)"rel",
			"alternate", (xmlChar *)"href")))       //ATOM
				link = g_strdup(_("No Information"));
	}

	id = (gchar *)layer_find (top, (gchar *)"id",				//ATOM
			layer_find (top, (gchar *)"guid", NULL));		//RSS 2.0
	feed = g_strdup_printf("%s\n", id ? id : link);
	if (feed) {
		g_strstrip(feed);
		if (article_uid != NULL)
			*article_uid = g_strdup(feed);
	}
	//not very nice but allows shortcutting
	if (feed_is_new(feed_name, feed)) {
		g_free(link);
		if (feed) g_free(feed);
		return NULL;
	}

	p = g_strdup(layer_find (top, "title", "Untitled article"));
	//firstly try to parse as an ATOM author
	//process person construct
	q1 = g_strdup(
			layer_find_innerhtml (top, "author", "name", NULL));
	q2 = g_strdup(
			layer_find_innerhtml (top, "author", "uri", NULL));
	q3 = g_strdup(
			layer_find_innerhtml (top, "author", "email", NULL));
	if (q1) {
		q1 = g_strdelimit(q1, "><", ' ');
		qsafe = encode_rfc2047(q1);
		if (q3)	{
			q3 = g_strdelimit(q3, "><", ' ');
			q = g_strdup_printf("%s <%s>", qsafe, q3);
				g_free(q1);
				if (q2) g_free(q2);
				g_free(q3);
			} else {
				if (q2)
					q2 = g_strdelimit(q2, "><", ' ');
				else
					q2 = g_strdup(q1);
				q = g_strdup_printf("%s <%s>", qsafe, q2);
				g_free(q1);
				g_free(q2);
			}
			g_free(qsafe);
	} else {			//then RSS or RDF
			xmlNodePtr source;
			source = layer_find_pos(top, "source", "author");
			//try the source construct
			//source = layer_find_pos(el->children, "source", "contributor");
			if (source != NULL)
				q = g_strdup(layer_find(source, "name", NULL));
			else {
				q = g_strdup(layer_find (top, "author",
					layer_find (top, "creator", NULL)));	//this catches dc:creator too. wrong!
			}
			if (q) {
				GString *s = rss_strip_html(q);
				q = s->str;
				g_string_free(s, 0);
			}
			//we might end with a subject containing nothing but spaces
			if (q) g_strstrip (q);

			/* empty creator? is this even legal
			 * search for the source then
			 * anything else might worth searching for
			 */
			if (!q || !strlen(q))
				q = g_strdup(layer_find_ns_tag(top, "dc", "source", NULL));


			if (q) {
				//evo will go crazy when it'll encounter ":" character
				//it probably enforces strict rfc2047 compliance
				q = g_strdelimit(q, "><:", ' ');
				qsafe = encode_rfc2047(q);
				tmp = g_strdup_printf("\"%s\" <\"%s\">", qsafe, q);
				g_free(q);
				g_free(qsafe);
				q = tmp;
				if (q2) g_free(q2);
				if (q3) g_free(q3);
			}
	}
		//FIXME this might need xmlFree when namespacing
		b = (gchar *)layer_find_tag (top, "content",		//we prefer content first		  <--
				layer_find_tag (top, "description",	//it seems description is rather shorten version of the content, so |
				layer_find_tag (top, "summary",
				NULL)));
		if (b && strlen(b))
			b = g_strstrip(b);
		else
			b = g_strdup(layer_find (top, "description",
				layer_find (top, "content",
				layer_find (top, "summary", NULL))));

		if (!b || !strlen(b))
			b = g_strdup(_("No information"));

		d = (gchar *)layer_find (top, "pubDate", NULL);
		//date in dc module format
		if (!d) {
			d2 = (gchar *)layer_find (top, "date", NULL);					//RSS2
			if (!d2) {
				d2 = (gchar *)layer_find(top, "published",
					layer_find(top, "updated", NULL));				//ATOM
				if (!d2) //take channel date if exists
					d2 = g_strdup(main_date);
			}
		}

		//<enclosure url=>
		//handle multiple enclosures
		encl = (gchar *)layer_find_innerelement(top, "enclosure", "url",	// RSS 2.0 Enclosure
			layer_find_innerelement(top, "link", "enclosure", NULL));		// ATOM Enclosure

		//handle screwed feeds that set url to "" (feed does not validate!)
		if (encl && !strlen(encl)) {
			g_free(encl);
			encl = NULL;
		}
		//handle attatchments (can be multiple)
		attachments = layer_find_tag_prop(top, "media", "url");
		if (!attachments)
			attachments = layer_query_find_all_prop (top,
				"link", (xmlChar *)"rel",
				"enclosure", (xmlChar *)"href");

//                char *comments = g_strdup(layer_find (top, "comments", NULL));	//RSS,
		comments = (gchar *)layer_find_ns_tag(top, "wfw", "commentRss", NULL); //add slash:comments
		tcat = (gchar *)layer_find_ns_tag(top, "dc", "subject", NULL);
		if (tcat)
			category = g_list_append(category, g_strdup(tcat));
		else
			category = layer_find_all(top, "category", NULL);

		d("link:%s\n", link);
		d("author:%s\n", q);
		d("title:%s\n", p);
		d("date:%s\n", d);
		d("date:%s\n", d2);
		d("body:%s\n", b);

		ftotal++;
		sp =  decode_html_entities (p);
		tmp = decode_utf8_entities (b);
		g_free(b);

		if (feed_name) {
			gchar *buff = process_images(tmp, base ? base : link, FALSE, NULL);
			g_free(tmp);
			b = buff;
		} else
			b = tmp;

		CF = g_new0(create_feed, 1);
		/* pack all data */
		CF->q		= g_strdup(q);
		CF->subj	= g_strdup(sp);
		CF->body	= g_strdup(b);
		CF->date	= g_strdup(d);
		CF->dcdate	= g_strdup(d2);
		CF->website	= g_strdup(link);
		CF->encl	= g_strdup(encl);
		CF->attachments	= attachments;
		CF->comments	= g_strdup(comments);
		CF->feed_fname  = g_strdup(feed_name);	//feed file name
		CF->feed_uri	= g_strdup(feed);	//feed uri (uid!)
		CF->category	= category;		//list of category feed is posted under
		g_free(comments);
		g_free(p);
		g_free(sp);
		if (q) g_free(q);
		g_free(b);
		if (feed) g_free(feed);
		if (encl) g_free(encl);
		g_free(link);
		return CF;

}

void
refresh_mail_folder(CamelFolder *mail_folder);

void
refresh_mail_folder(CamelFolder *mail_folder)
{
	mail_refresh_folder(mail_folder, NULL, NULL);
#if (DATASERVER_VERSION >= 2033001)
		camel_folder_synchronize (mail_folder, FALSE, G_PRIORITY_DEFAULT,
			NULL, NULL, NULL);
#else
	camel_folder_sync(mail_folder, FALSE, NULL);
#endif
	camel_folder_thaw(mail_folder);
}

GQueue *
display_channel_items_sync(AsyncData *asyncr)
{
	FILE *fr, *fw;
	RDF *r = asyncr->r;
	GQueue *status_msg = asyncr->status_msg;
	GtkWidget *progress = r->progress;
	gchar *chn_name = r->title;
	gchar *url = r->uri;
	GArray *item = r->item;
	CamelFolder *mail_folder = NULL;
	gchar *sender;
	gboolean freeze = FALSE;
	gchar *article_uid = NULL;
	gchar *safes, *feed_dir, *feed_name;
	gchar *msg, *safchn;
	create_feed *CF;
	gchar *subj;
	xmlNodePtr el;
	guint i;

	safes = encode_rfc2047(chn_name);
	safchn = g_strchomp (g_strdup(chn_name));
	sender = g_strdup_printf("%s <%s>", safes, safchn);
	g_free(safchn);
	g_free(safes);

	migrate_crc_md5(chn_name, url);
	r->feedid = gen_md5(url);
	feed_dir = rss_component_peek_base_directory();
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);

	feed_name = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", feed_dir, r->feedid);
	g_free(feed_dir);

	fr = fopen(feed_name, "r");
	fw = fopen(feed_name, "a+");

	for (i=0; NULL != (el = g_array_index(item, xmlNodePtr, i)); i++) {
		update_progress_text(chn_name);
		if (rf->cancel || rf->cancel_all || rf->display_cancel)
			break;

		if (progress) {
			gdouble fraction = (gdouble)i/item->len;
			gtk_progress_bar_set_fraction(
					(GtkProgressBar *)progress,
					fraction);
			msg = g_strdup_printf("%2.0f%% done", fraction*100);
			gtk_progress_bar_set_text(
					(GtkProgressBar *)progress,
					msg);
			g_free(msg);
		}
		if (!r->uids) {
			r->uids = g_array_new(TRUE, TRUE, sizeof(gpointer));
		}

		CF = parse_channel_line(el->children, feed_name,
			r, &article_uid);
		g_array_append_val(r->uids, article_uid);
		if (!CF) continue;
		CF->feedid = g_strdup(r->feedid);
		CF->sender = g_strdup(sender);
		if (r->prefix)
			CF->full_path = g_build_path(
						G_DIR_SEPARATOR_S, r->prefix,
						chn_name, NULL);
		else
			CF->full_path = g_strdup(chn_name);

		if (!mail_folder) {
			mail_folder = check_feed_folder(CF->full_path);
			if (!mail_folder)
				goto out;
		}
		subj = g_strdup(CF->subj);

		ftotal++;
		if (gconf_client_get_bool(rss_gconf, GCONF_KEY_DOWNLOAD_ENCLOSURES, NULL)) {
			if (CF->encl) {
				process_enclosure(CF);
				goto done;
			} else if (g_list_length(CF->attachments)) {
				process_attachments(CF);
				goto done;
			}
		}

		if (!freeze) {
			camel_folder_freeze(mail_folder);
			freeze = TRUE;
		}
		create_mail(CF);
		write_feed_status_line(
			CF->feed_fname, CF->feed_uri);
		free_cf(CF);

done:		farticle++;
		d("put success()\n");
		update_status_icon_text(status_msg, chn_name, subj);
		g_free(subj);
	}
	if (freeze)
		refresh_mail_folder(mail_folder);

	if (mail_folder) {
		asyncr->mail_folder = mail_folder;
	}
out:	g_free(sender);

	if (fr) fclose(fr);
	if (fw) fclose(fw);

	g_free(feed_name);
	return status_msg;
}

static void
display_channel_items_thread (GSimpleAsyncResult *simple,
	GObject *object,
	GCancellable *cancellable)
{
	AsyncData *asyncr;
	GError *error = NULL;

	asyncr = g_simple_async_result_get_op_res_gpointer (simple);
	asyncr->status_msg = display_channel_items_sync(asyncr);

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}
}

void
asyncr_context_free(AsyncData *asyncr)
{
	d("free r-> components\n");
	if (asyncr->r->maindate)
		g_free(asyncr->r->maindate);
	g_array_free(asyncr->r->item, TRUE);
	g_free(asyncr->r->feedid);
	if (asyncr->r->uids)
		g_array_free(asyncr->r->uids, TRUE);
	if (asyncr->r->cache)
		xmlFreeDoc(asyncr->r->cache);
	if (asyncr->r->type)
		g_free(asyncr->r->type);
	if (asyncr->r->version)
		g_free(asyncr->r->version);
	g_free(asyncr->r);
	g_free(asyncr);
}


void
display_channel_items (RDF *r,
	gint io_priority,
	GCancellable *cancellable,
	GAsyncReadyCallback callback,
	gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncData *data;

	data = g_new0(AsyncData, 1);
	data->r = r;
	data->status_msg = user_data;

	simple = g_simple_async_result_new (
		NULL, callback, user_data, display_channel_items);

	g_simple_async_result_set_op_res_gpointer (
		simple, data, (GDestroyNotify)asyncr_context_free);

	g_simple_async_result_run_in_thread (
		simple, display_channel_items_thread, io_priority, cancellable);

	g_object_unref (simple);
}

gchar *
encode_html_entities(gchar *str)
{
	xmlChar *tmp;

	g_return_val_if_fail (str != NULL, NULL);

	tmp =  xmlEncodeEntitiesReentrant(NULL, (xmlChar *)str);
	return (gchar *)tmp;
}

gchar *
decode_html_entities(gchar *str)
{
	gchar *newstr;
	xmlChar *tmp;
	xmlParserCtxtPtr ctxt = xmlNewParserCtxt();

	g_return_val_if_fail (str != NULL, NULL);

	xmlCtxtUseOptions(
		ctxt,
		XML_PARSE_RECOVER
		| XML_PARSE_NOENT
		| XML_PARSE_NOERROR
		| XML_PARSE_NONET);

	tmp =  xmlStringDecodeEntities(
		ctxt,
		BAD_CAST str,
		XML_SUBSTITUTE_REF
		&XML_SUBSTITUTE_PEREF,
		0,
		0,
		0);

	newstr = g_strdup((gchar *)tmp);
	xmlFree(tmp);
	xmlFreeParserCtxt(ctxt);
	return newstr;
}

gchar *
decode_entities(gchar *source)
{
	GString *str = g_string_new(NULL);
	GString *res = g_string_new(NULL);
	gchar *result;
	const unsigned char *s;
	guint len;
	int in=0, out=0;
	int state, pos;

	g_string_append(res, source);
reent:  s = (const unsigned char *)res->str;
	len = strlen(res->str);
	state = 0;
	pos = 1;
	g_string_truncate(str, 0);
	while (*s != 0 || len) {
		if (state) {
			if (*s==';') {
				state = 2; //entity found
				out = pos;
				break;
			} else {
				g_string_append_c(str, *s);
			}
		}
		if (*s=='&') {
			in = pos-1;
			state = 1;
		}
		s++;
		pos++;
		len--;
	}
	if (state == 2) {
		htmlEntityDesc *my = (htmlEntityDesc *)htmlEntityLookup((xmlChar *)str->str);
		if (my) {
			g_string_erase(res, in, out-in);
			g_string_insert_unichar(res, in, my->value);
			result = res->str;
			g_string_free(res, FALSE);
			res = g_string_new(NULL);
			g_string_append(res, result);
			goto reent;
		}
	}
	result = res->str;
	g_string_free(res, FALSE);
	return result;
}

gchar *
decode_utf8_entities(gchar *str)
{
	int inlen, utf8len;
	gchar *buffer;
	g_return_val_if_fail (str != NULL, NULL);

	inlen = strlen(str);
	utf8len = 5*inlen+1;
	buffer = g_malloc0(utf8len);
	UTF8ToHtml((unsigned char *)buffer, &utf8len, (unsigned char *)str, &inlen);
	return buffer;
}

