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

#include <string.h>
#include <glib/gi18n-lib.h>

#include <libxml/parserInternals.h>
#include <libxml/xmlmemory.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/debugXML.h>

#include "fetch.h"
#include "rss.h"
#include "parser.h"
#include "misc.h"

#define d(x)

/************ RDF Parser *******************/

guint rsserror = FALSE;
gchar *rssstrerror = NULL;
extern rssfeed *rf;

//
// decodes url_encoded strings that might appear in a html body
//
xmlDoc *
rss_html_url_decode(const char *html, int len)
{
        xmlDoc *src = NULL;
        xmlDoc *doc = NULL;
        gchar *url, *tmpurl;

        src = (xmlDoc *)parse_html_sux(html, len);

        if (!src)
                return NULL;

        doc = src;

        while ((doc = (xmlDoc *)html_find((xmlNode *)doc, "img"))) {
                if ((url = (gchar *)xmlGetProp((xmlNodePtr)doc, (xmlChar *)"src"))) {
			tmpurl = camel_url_decode_path(strstr(url, "http:"));
			xmlSetProp((xmlNodePtr)doc, (xmlChar *)"src", (xmlChar *)tmpurl);
			g_free(tmpurl);
		}
	}
	return src;
}

void
html_set_base(xmlNode *doc, char *base, char *tag, char *prop, char *basehref)
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
        while ((doc = html_find((xmlNode *)doc, tag))) {
                if ((url = (gchar *)xmlGetProp(doc, (xmlChar *)prop))) {
                        if (!strncmp(tag, "img", 3) && !strncmp(prop, "src", 3)) {
                                gchar *tmpurl = strplchr(url);
                                xmlSetProp(doc, (xmlChar *)prop, (xmlChar *)tmpurl);
                                g_free(tmpurl);
                        }
                        d(g_print("DEBUG: parsing: %s\n", url));
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
//	rssstrerror
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
        xmlDoc *doc;
	rsserror = FALSE;
	rssstrerror = NULL;

        g_return_val_if_fail (buf != NULL, NULL);

        if (!sax) {
                xmlInitParser();
                sax = xmlMalloc (sizeof (xmlSAXHandler));
//#if LIBXML_VERSION > 20600 
                xmlSAXVersion (sax, 2);
//#else
  //              memcpy (sax, &xmlDefaultSAXHandler, sizeof (xmlSAXHandler));
//#endif
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
//#if LIBXML_VERSION > 20600
        ctxt->sax2 = 1;
        ctxt->str_xml = xmlDictLookup (ctxt->dict, BAD_CAST "xml", 3);
        ctxt->str_xmlns = xmlDictLookup (ctxt->dict, BAD_CAST "xmlns", 5);
        ctxt->str_xml_ns = xmlDictLookup (ctxt->dict, XML_XML_NAMESPACE, 36);
//#endif

        ctxt->recovery = TRUE;
        ctxt->vctxt.error = my_xml_parser_error_handler;
        ctxt->vctxt.warning = my_xml_parser_error_handler;

        xmlCtxtUseOptions(ctxt, XML_PARSE_DTDLOAD
                                | XML_PARSE_NOENT);

//                                | XML_PARSE_NOCDATA);

        xmlParseDocument (ctxt);

        doc = ctxt->myDoc;
        ctxt->sax = NULL;
        xmlFreeParserCtxt (ctxt);

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

        src = (xmlDoc *)parse_html_sux(html, len);

        if (!src)
                return NULL;
        doc = src;
        gchar *newbase = NULL;
        newbase = (gchar *)xmlGetProp(html_find((xmlNode *)doc, "base"), (xmlChar *)"href");
        d(g_print("newbase:|%s|\n", newbase));
        xmlDoc *tmpdoc = (xmlDoc *)html_find((xmlNode *)doc, "base");
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

char *
layer_find_innerelement (xmlNodePtr node, 
	    char *match, char *el,
	    char *fail)
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
            char *match)
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

/* returns node disregarding type
 */
char *
layer_find (xmlNodePtr node, 
	    char *match, 
	    char *fail)
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
	    char *match, 
	    char *fail)
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

gchar *standard_rss_modules[5][3] = {
	{"content", "content", (gchar *)content_rss},
	{"dublin core", "dc", (gchar *)dublin_core_rss},
	{"syndication", "sy", (gchar *)syndication_rss},
	{"well formed web", "wfw", (gchar *)wfw_rss},
	{"slashdot entities", "slash", (gchar *)dublin_core_rss}};

//<nsmatch:match>content</nsmatch:match>
static char*
layer_find_ns_tag(xmlNodePtr node,
		char *nsmatch,
		char *match,
		char *fail)
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
char *
layer_find_tag (xmlNodePtr node,
            char *match,
            char *fail)
{
	xmlBufferPtr buf = xmlBufferCreate();
	gchar *content;
	guint len = 0;
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
 						len = xmlNodeDump(buf, node->doc, node, 0, 0);
						content = g_strdup_printf("%s", xmlBufferContent(buf));
						xmlBufferFree(buf);
					} else {
						content = (char *)xmlNodeGetContent(node);
					}
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
	g_print("media_rss()\n");

	content = (gchar *)xmlGetProp(node, (xmlChar *)search);
	if (content)
		return content;
	else
		return fail;
}

gchar *property_rss_modules[1][3] = {
	{"media", "media", (gchar *)media_rss}};

char *
layer_find_tag_prop (xmlNodePtr node,
            char *match,
            char *search,
            char *fail)
{
	int i;
	char* (*func)();

        while (node!=NULL) {
#ifdef RDF_DEBUG
                xmlDebugDumpNode (stdout, node, 32);
                printf("%s.\n", node->name);
#endif
		if (node->ns && node->ns->prefix)
		{
			for (i=0; i < 1; i++)
			{
				if (!strcasecmp ((char *)node->ns->prefix, property_rss_modules[i][1]))
				{
					func = (gpointer)property_rss_modules[i][2];
					if (strcasecmp ((char *)node->ns->prefix, match)==0)
					{
						g_print("URL:%s\n", func(node, search, fail));
					}
				}
			}
		}
                node = node->next;
	}
	return fail;
}

gchar *
layer_find_innerhtml (xmlNodePtr node,
	    char *match, char *submatch,
	    char *fail)
{
	while (node!=NULL) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (strcasecmp ((char *)node->name, match)==0 && node->children) {
			return layer_find(node->children->next, submatch, fail);
		}
		node = node->next;
	}
	return fail;
}

xmlNodePtr
layer_find_pos (xmlNodePtr node,
            char *match, char *submatch)
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
                		if (strcasecmp ((char *)subnode->name, submatch)==0 && subnode->children)
				{
                        			return subnode->children->next;
				}
				subnode = subnode->next;
			}
                }
                node = node->next;
        }
        return NULL;
}

char *
layer_find_url (xmlNodePtr node, 
		char *match, 
		char *fail)
{
	char *p = layer_find (node, match, fail);
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
	xmlNodePtr channel = NULL;
	xmlNodePtr image = NULL;
	GArray *item = g_array_new (TRUE, TRUE, sizeof (xmlNodePtr));
	char *t;
	char *charset;

	/* check in-memory encoding first, fallback to transport encoding, which may or may not be correct */
	if (r->cache->charset == XML_CHAR_ENCODING_UTF8
	    || r->cache->charset == XML_CHAR_ENCODING_ASCII) {
		charset = NULL;
	} else {
		/* bad/missing encoding, fallback to latin1 (locale?) */
		charset = r->cache->encoding ? (char *)r->cache->encoding : "iso-8859-1";
	}

	do {
		walk = rewalk;
		rewalk = NULL;
		
		while (walk!=NULL){
#ifdef RDF_DEBUG
			printf ("%p, %s\n", walk, walk->name);
#endif
			if (strcasecmp ((char *)walk->name, "rdf") == 0) {
//				xmlNode *node = walk;
				rewalk = walk->children;
				walk = walk->next;
				if (!r->type)
					r->type = g_strdup("RDF");
				r->type_id = RDF_FEED;
//                		gchar *ver = xmlGetProp(node, "version");
                		if (r->version)
					g_free(r->version);
				r->version = g_strdup("(RSS 1.0)");
//				if (ver)
//					xmlFree(ver);
				continue;
			}
			if (strcasecmp ((char *)walk->name, "rss") == 0){
				xmlNode *node = walk;
				rewalk = walk->children;
				walk = walk->next;
				if (!r->type)
				r->type = g_strdup("RSS");
				r->type_id = RSS_FEED;
                		gchar *ver = (gchar *)xmlGetProp(node, (xmlChar *)"version");
                		if (r->version)
					g_free(r->version);
				r->version = g_strdup(ver);
				if (ver)
					xmlFree(ver);
				continue;
			}
			if (strcasecmp ((char *)walk->name, "feed") == 0) {
				xmlNode *node = walk;
				if (!r->type)
				r->type = g_strdup("ATOM");
				r->type_id = ATOM_FEED;
                		gchar *ver = (gchar *)xmlGetProp(node, (xmlChar *)"version");
				if (ver)
				{
                			if (r->version)
						g_free(r->version);
					r->version = g_strdup(ver);
					xmlFree(ver);
				}
				else
				{
                			if (r->version)
						g_free(r->version);
					r->version = g_strdup("1.0");
				}
			}

			/* This is the channel top level */
#ifdef RDF_DEBUG
			printf ("Top level '%s'.\n", walk->name);
#endif
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
	}
	while (rewalk);
	
	if (channel == NULL) {
		fprintf(stderr, "ERROR:No channel definition.\n");
		return NULL;
	}
//	gchar *server = get_server_from_uri(r->uri);
//	gchar *fav = g_strconcat(server, "/favicon.ico", NULL);
//	g_free(server);

	if (image != NULL)
		r->image = layer_find(image->children, "url", NULL);

//	g_print("status image:%d\n", net_get_status(r->image, NULL));
//	if (404 == net_get_status(r->image, NULL))
//		r->image = NULL;

//	g_free(fav);

	t = g_strdup(get_real_channel_name(r->uri, NULL));
	//feed might be added with no validation
	//so it could be named Untitled channel
	//till validation process
	if (t == NULL || !g_ascii_strncasecmp(t,
			DEFAULT_NO_CHANNEL, 
			strlen(DEFAULT_NO_CHANNEL))) 
	{
		
		t = layer_find(channel->children, 
				"title", 
				DEFAULT_NO_CHANNEL);
		t = decode_html_entities(t);	
		gchar *tmp = sanitize_folder(t);
		g_free(t);
		t = tmp;
		t = generate_safe_chn_name(t);
	}
	gchar *tmp = layer_find(channel->children, "ttl", NULL);
	if (tmp)
		r->ttl = atoi(tmp);
	else
		r->ttl = 0;

	//items might not have a date
	// so try to grab channel/feed date
	gchar *md2 = g_strdup(layer_find(channel->children, "date", 
		layer_find(channel->children, "pubDate", 
		layer_find(channel->children, "updated", NULL))));
	r->maindate = md2;
	r->total = item->len;
	r->item = item;
	r->title = t;
	return r->title;
}

create_feed *
parse_channel_line(xmlNode *top, gchar *feed_name, char *main_date)
{
	char *q = NULL;
	char *b = NULL;
	char *d2 = NULL;
	gchar *feed = NULL;
	gchar *encl;
	xmlChar *buff = NULL;
	guint size = 0;
	GList *category = NULL;

	char *p = g_strdup(layer_find (top, "title", "Untitled article"));
	//firstly try to parse as an ATOM author
	//process person construct
	char *q1 = g_strdup(layer_find_innerhtml (top, "author", "name", NULL));
	char *q2 = g_strdup(layer_find_innerhtml (top, "author", "uri", NULL));
	char *q3 = g_strdup(layer_find_innerhtml (top, "author", "email", NULL));
	if (q1) {
		q1 = g_strdelimit(q1, "><", ' ');
		gchar *qsafe = encode_rfc2047(q1);
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
			else
               			q = g_strdup(layer_find (top, "author", 
				layer_find (top, "creator", NULL)));
			if (q) {
				//evo will go crazy when it'll encounter ":" character
        			//it probably enforces strict rfc2047 compliance
        			q = g_strdelimit(q, "><:", ' ');
				gchar *qsafe = encode_rfc2047(q);
        			gchar *tmp = g_strdup_printf("\"%s\" <\"%s\">", qsafe, q);
				g_free(q);
				g_free(qsafe);
				q = tmp;
				if (q2) g_free(q2);
				if (q3) g_free(q3);
			}
		}
		//FIXME this might need xmlFree when namespacing
		b = layer_find_tag (top, "content",			//we prefer content first					  <--
				layer_find_tag (top, "description", 	//it seems description is rather shorten version of the content, so |
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

                char *d = layer_find (top, "pubDate", NULL);
		//date in dc module format
		if (!d) {
                	d2 = layer_find (top, "date", NULL);					//RSS2
			if (!d2) {
				d2 = layer_find(top, "updated", NULL); 			//ATOM
				if (!d2) //take channel date if exists
					d2 = g_strdup(main_date);
			}
		}

		//<enclosure url=>
		//handle multiple enclosures
		encl = layer_find_innerelement(top, "enclosure", "url",	// RSS 2.0 Enclosure
			layer_find_innerelement(top, "link", "enclosure", NULL)); 		// ATOM Enclosure
//		encl = layer_find_tag_prop(el->children, "media", "url",	// RSS 2.0 Enclosure
//							NULL); 		// ATOM Enclosure
		//we have to free this somehow
		//<link></link>
                char *link = g_strdup(layer_find (top, "link", NULL));		//RSS,
		if (!link) 								// <link href=>
			link = layer_find_innerelement(top, "link", "href", 
							g_strdup(_("No Information")));	//ATOM

//                char *comments = g_strdup(layer_find (top, "comments", NULL));	//RSS,
		char *comments = NULL;
//		if (!comments)
			comments = layer_find_ns_tag(top, "wfw", "commentRss", NULL); //add slash:comments
		gchar *tcat = layer_find_ns_tag(top, "dc", "subject", NULL);
		if (tcat)
			category = g_list_append(category, g_strdup(tcat));
		else
			category = layer_find_all(top, "category", NULL);

		char *id = layer_find (top, "id",				//ATOM
				layer_find (top, "guid", NULL));		//RSS 2.0
		feed = g_strdup_printf("%s\n", id ? id : link);
		g_strstrip(feed);
		d(g_print("link:%s\n", link));
		d(g_print("author:%s\n", q));
		d(g_print("title:%s\n", p));
		d(g_print("date:%s\n", d));
		d(g_print("date:%s\n", d2));
		d(g_print("body:%s\n", b));

		//not very nice but prevents unnecessary long body processing
		if (!feed_is_new(feed_name, feed)) {
                        ftotal++;
                        p =  decode_html_entities (p);
                        gchar *tmp = decode_utf8_entities(b);
                        g_free(b);

			if (feed_name) {
                        	xmlDoc *src = (xmlDoc *)parse_html_sux(tmp, strlen(tmp));
                        	if (src) {
                                	xmlNode *doc = (xmlNode *)src;

                                	while ((doc = html_find(doc, "img"))) {
                                        	gchar *name = NULL;
                                        	xmlChar *url = xmlGetProp(doc, (xmlChar *)"src");
                                        	if (url) {
                                                	if ((name = fetch_image((gchar *)url, link))) {
                                                        	xmlSetProp(doc, (xmlChar *)"src", (xmlChar *)name);
								g_free(name);
							}
                                                	xmlFree(url);
                                        	}
                                	}
                                	xmlDocDumpMemory(src, &buff, (int*)&size);
                                	xmlFree(src);
                        	}
                        	g_free(tmp);
                        	b=(gchar *)buff;
			}
			else
				b = tmp;
		}

		create_feed *CF = g_new0(create_feed, 1);	
		/* pack all data */
		CF->q 		= g_strdup(q);
		CF->subj 	= g_strdup(p);
		CF->body 	= g_strdup(b);
		CF->date 	= g_strdup(d);
		CF->dcdate 	= g_strdup(d2);
		CF->website 	= g_strdup(link);
		CF->encl 	= g_strdup(encl);
		CF->comments 	= g_strdup(comments);
		CF->feed_fname  = g_strdup(feed_name);	//feed file name
		CF->feed_uri	= g_strdup(feed);	//feed uri (uid!)
		CF->category	= category;		//list of category feed is posted under
		g_free(p);
		if (q) g_free(q);
		g_free(b);
		if (feed) g_free(feed);
		if (encl) g_free(encl);
		g_free(link);
		return CF;

}

gchar *
update_channel(RDF *r)
{
        guint i;
	gchar *sender;
	xmlNodePtr el;
	gchar *subj;
	create_feed *CF;
	gchar *chn_name = r->title;
	gchar *url = r->uri;
	gchar *main_date = r->maindate;
	GArray *item = r->item;
	GtkWidget *progress = r->progress;

	gchar *safes = encode_rfc2047(chn_name);

	sender = g_strdup_printf("%s <%s>", safes, chn_name);
	g_free(safes);

	migrate_crc_md5(chn_name, url);

	gchar *buf = gen_md5(url);

	gchar *feed_dir = rss_component_peek_base_directory(mail_component_peek());
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
	    g_mkdir_with_parents (feed_dir, 0755);

	gchar *feed_name = g_strdup_printf("%s/%s", feed_dir, buf);
	g_free(feed_dir);
	
	FILE *fr = fopen(feed_name, "r");
	FILE *fw = fopen(feed_name, "a+");
	//int fw = g_open (feed_name, O_WRONLY | O_CREAT| O_APPEND | O_BINARY, 0666);
	
	for (i=0; NULL != (el = g_array_index(item, xmlNodePtr, i)); i++) {
		update_sr_message();
		if (rf->cancel) goto out;

		if (progress) {
			gdouble fraction = (gdouble)i/item->len;
                	gtk_progress_bar_set_fraction((GtkProgressBar *)progress, fraction);
			gchar *msg = g_strdup_printf("%2.0f%% done", fraction*100);
                	gtk_progress_bar_set_text((GtkProgressBar *)progress, msg);
			g_free(msg);
		}

		CF = parse_channel_line(el->children, feed_name, main_date);
		if (!r->uids) {
			r->uids = g_array_new(TRUE, TRUE, sizeof(gpointer));
		}
		gchar *uid = g_strdup(CF->feed_uri);
		g_array_append_val(r->uids, uid);
		CF->feedid 	= g_strdup(buf);
		CF->sender 	= g_strdup(sender);
		if (r->prefix)
			CF->full_path	= g_strconcat(r->prefix, "/", chn_name, NULL);
		else
			CF->full_path 	= g_strdup(chn_name);

		subj = CF->subj;

		while (gtk_events_pending())
                  gtk_main_iteration ();

		if (!feed_is_new(feed_name, CF->feed_uri)) {
			ftotal++;
			if (CF->encl) {
				GError *err = NULL;
				fetch_unblocking(
                        	        CF->encl,
                        	        textcb,
                                	NULL,
                                	(gpointer)finish_enclosure,
                                	CF,
					0,
                                	&err);
			} else {
   	    	    		create_mail(CF);
				write_feed_status_line(CF->feed_fname, CF->feed_uri);
				free_cf(CF);
			}
			farticle++;
			d(g_print("put success()\n"));
			update_status_icon(chn_name, subj);
		} else
			free_cf(CF);
        }
out:	g_free(sender);

	if (fr) fclose(fr);
	if (fw) fclose(fw);
	
	g_free(feed_name);
	return buf;
}

gchar *
encode_html_entities(gchar *str)
{
        g_return_val_if_fail (str != NULL, NULL);

/*        xmlParserCtxtPtr ctxt = xmlNewParserCtxt();
        xmlCtxtUseOptions(ctxt,   XML_PARSE_RECOVER
                                | XML_PARSE_NOENT
                                | XML_PARSE_NOERROR
                                | XML_PARSE_NONET);*/

        xmlChar *tmp =  xmlEncodeEntitiesReentrant(NULL, (xmlChar *)str);

/*        xmlChar *tmp =  (gchar *)xmlStringDecodeEntities(ctxt,
                                             BAD_CAST str,
                                             XML_SUBSTITUTE_REF
                                             & XML_SUBSTITUTE_PEREF,
                                             0,
                                             0,
                                             0);

        newstr = g_strdup(tmp);
        xmlFree(tmp);
        xmlFreeParserCtxt(ctxt);
        return newstr;*/
        return (gchar *)tmp;
}

gchar *
decode_html_entities(gchar *str)
{
        gchar *newstr;
        g_return_val_if_fail (str != NULL, NULL);

        xmlParserCtxtPtr ctxt = xmlNewParserCtxt();
        xmlCtxtUseOptions(ctxt,   XML_PARSE_RECOVER
                                | XML_PARSE_NOENT
                                | XML_PARSE_NOERROR
                                | XML_PARSE_NONET);

        xmlChar *tmp =  xmlStringDecodeEntities(ctxt,
                                             BAD_CAST str,
                                             XML_SUBSTITUTE_REF
                                             & XML_SUBSTITUTE_PEREF,
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
                        gchar *result = res->str;
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

