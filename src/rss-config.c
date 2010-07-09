/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2010 Lucian Langa <cooly@gnome.eu.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <stdio.h>
#include <string.h>

#include "misc.h"
#include "rss-config.h"

extern rssfeed *rf;
extern GConfClient *rss_gconf;

GSList *rss_list = NULL;

static gboolean
xml_set_content (xmlNodePtr node, char **val)
{
	char *buf;
	int res;

	buf = (char *)xmlNodeGetContent(node);
	if (buf == NULL) {
		res = (*val != NULL);
		if (res) {
			g_free(*val);
			*val = NULL;
		}
	} else {
		res = *val == NULL || strcmp(*val, buf) != 0;
		if (res) {
			g_free(*val);
			*val = g_strdup(buf);
		}
		xmlFree(buf);
	}

	return res;
}

static gboolean
xml_set_prop (xmlNodePtr node, const char *name, char **val)
{
	char *buf;
	int res;

	buf = (char *)xmlGetProp (node, (xmlChar *)name);
	if (buf == NULL) {
		res = (*val != NULL);
		if (res) {
			g_free(*val);
			*val = NULL;
		}
	} else {
		res = *val == NULL || strcmp(*val, buf) != 0;
		if (res) {
			g_free(*val);
			*val = g_strdup(buf);
		}
		xmlFree(buf);
	}

	return res;
}

static gboolean
xml_set_bool (xmlNodePtr node, const char *name, gboolean *val)
{
	gboolean gbool;
	char *buf;

	if ((buf = (char *)xmlGetProp (node, (xmlChar *)name))) {
		gbool = (!strcmp (buf, "true") || !strcmp (buf, "yes"));
		xmlFree (buf);

		if (gbool != *val) {
			*val = gbool;
			return TRUE;
		}
	}

	return FALSE;
}

gboolean
feed_new_from_xml(char *xml)
{
	xmlNodePtr node;
	xmlDocPtr doc = NULL;
	char *uid = NULL;
	char *name = NULL;
	char *url = NULL;
	char *type = NULL;
	gboolean enabled = FALSE;
	gboolean html = FALSE;
	guint del_feed=0;
	guint del_days=0;
	guint del_messages=0;
	guint del_unread=0, del_notpresent=0;
	guint ttl=0;
	guint ttl_multiply=0;
	guint update=0;
	gchar *ctmp = NULL;

	if (!(doc = xmlParseDoc ((xmlChar *)xml)))
		return FALSE;

	node = doc->children;
	if (strcmp ((char *)node->name, "feed") != 0) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	xml_set_prop (node, "uid", &uid);
	xml_set_bool (node, "enabled", &enabled);
	xml_set_bool (node, "html", &html);

	for (node = node->children; node; node = node->next) {
		if (!strcmp ((char *)node->name, "name")) {
			xml_set_content (node, &name);
		}
		if (!strcmp ((char *)node->name, "url")) {
			xml_set_content (node, &url);
		}
		if (!strcmp ((char *)node->name, "type")) {
			xml_set_content (node, &type);
		}
		if (!strcmp ((char *)node->name, "delete")) {
			xml_set_prop (node, "option", &ctmp);
			del_feed = atoi(ctmp);
			xml_set_prop (node, "days", &ctmp);
			del_days = atoi(ctmp);
			xml_set_prop (node, "messages", &ctmp);
			del_messages = atoi(ctmp);
			xml_set_bool (
				node,
				"unread",
				(gboolean *)&del_unread);
			xml_set_bool (
				node,
				"notpresent",
				(gboolean *)&del_notpresent);
		}
		if (!strcmp ((char *)node->name, "ttl")) {
			xml_set_prop (node, "option", &ctmp);
			update = atoi(ctmp);
			xml_set_prop (node, "value", &ctmp);
			ttl = atoi(ctmp);
			xml_set_prop (node, "factor", &ctmp);
			if (ctmp)
				ttl_multiply = atoi(ctmp);
			if (ctmp) g_free(ctmp);
		}
	}

	g_hash_table_insert(rf->hrname, name, uid);
	g_hash_table_insert(
		rf->hrname_r,
		g_strdup(uid),
		g_strdup(name));
	g_hash_table_insert(
		rf->hr,
		g_strdup(uid),
		url);
	g_hash_table_insert(
		rf->hrh,
		g_strdup(uid),
		GINT_TO_POINTER(html));
	g_hash_table_insert(
		rf->hrt,
		g_strdup(uid),
		type);
	g_hash_table_insert(
		rf->hre,
		g_strdup(uid),
		GINT_TO_POINTER(enabled));
	g_hash_table_insert(
		rf->hrdel_feed,
		g_strdup(uid),
		GINT_TO_POINTER(del_feed));
	g_hash_table_insert(
		rf->hrdel_days,
		g_strdup(uid),
		GINT_TO_POINTER(del_days));
	g_hash_table_insert(
		rf->hrdel_messages,
		g_strdup(uid),
		GINT_TO_POINTER(del_messages));
	g_hash_table_insert(
		rf->hrdel_unread,
		g_strdup(uid),
		GINT_TO_POINTER(del_unread));
	g_hash_table_insert(
		rf->hrdel_notpresent,
		g_strdup(uid),
		GINT_TO_POINTER(del_notpresent));
	g_hash_table_insert(
		rf->hrupdate,
		g_strdup(uid),
		GINT_TO_POINTER(update));
	g_hash_table_insert(
		rf->hrttl,
		g_strdup(uid),
		GINT_TO_POINTER(ttl));
	g_hash_table_insert(
		rf->hrttl_multiply,
		g_strdup(uid),
		GINT_TO_POINTER(ttl_multiply));
	xmlFreeDoc (doc);
	return TRUE;
}

char *
feeds_uid_from_xml (const char *xml)
{
	xmlNodePtr node;
	xmlDocPtr doc;
	char *uid = NULL;

	if (!(doc = xmlParseDoc ((xmlChar *)xml)))
		return NULL;

	node = doc->children;
	if (strcmp ((char *)node->name, "feed") != 0) {
		xmlFreeDoc (doc);
		return NULL;
	}

	xml_set_prop (node, "uid", &uid);
	xmlFreeDoc (doc);

	return uid;
}

gchar *
feed_to_xml(gchar *key)
{
	xmlNodePtr root, src;
	char *tmp;
	xmlChar *xmlbuf;
	xmlDocPtr doc;
	int n;
	gchar *ctmp;

	doc = xmlNewDoc ((xmlChar *)"1.0");

	root = xmlNewDocNode (doc, NULL, (xmlChar *)"feed", NULL);
	xmlDocSetRootElement (doc, root);

	xmlSetProp (
		root,
		(xmlChar *)"uid",
		(xmlChar *)(g_hash_table_lookup(rf->hrname, key)));
	xmlSetProp (
		root,
		(xmlChar *)"enabled",
		(xmlChar *)(g_hash_table_lookup(
				rf->hre,
				lookup_key(key)) ? "true" : "false"));
	xmlSetProp (
		root,
		(xmlChar *)"html",
		(xmlChar *)(g_hash_table_lookup(
				rf->hrh,
				lookup_key(key)) ? "true" : "false"));

	xmlNewTextChild (root, NULL, (xmlChar *)"name", (xmlChar *)key);
	xmlNewTextChild (
		root, NULL, (xmlChar *)"url",
		(xmlChar *)g_hash_table_lookup(rf->hr, lookup_key(key)));
	xmlNewTextChild (
		root, NULL, (xmlChar *)"type",
		(xmlChar *)g_hash_table_lookup(rf->hrt, lookup_key(key)));

	src = xmlNewTextChild (root, NULL, (xmlChar *)"delete", NULL);
	ctmp = g_strdup_printf(
		"%d",
		GPOINTER_TO_INT(
			g_hash_table_lookup(
				rf->hrdel_feed,
				lookup_key(key))));
	xmlSetProp (src, (xmlChar *)"option", (xmlChar *)ctmp);
	g_free(ctmp);
	ctmp = g_strdup_printf(
		"%d",
		GPOINTER_TO_INT(
			g_hash_table_lookup(
				rf->hrdel_days,
				lookup_key(key))));
	xmlSetProp (src, (xmlChar *)"days", (xmlChar *)ctmp);
	g_free(ctmp);
	ctmp = g_strdup_printf(
		"%d",
		GPOINTER_TO_INT(
			g_hash_table_lookup(
				rf->hrdel_messages,
				lookup_key(key))));
	xmlSetProp (src, (xmlChar *)"messages", (xmlChar *)ctmp);
	g_free(ctmp);
	xmlSetProp (
		src,
		(xmlChar *)"unread",
		(xmlChar *)(g_hash_table_lookup(
				rf->hrdel_unread,
				lookup_key(key)) ? "true" : "false"));
	xmlSetProp (
		src,
		(xmlChar *)"notpresent",
		(xmlChar *)(g_hash_table_lookup(
				rf->hrdel_notpresent,
				lookup_key(key)) ? "true" : "false"));

	src = xmlNewTextChild (root, NULL, (xmlChar *)"ttl", NULL);
	ctmp = g_strdup_printf(
		"%d",
		GPOINTER_TO_INT(g_hash_table_lookup(
				rf->hrupdate,
				lookup_key(key))));
	xmlSetProp (src, (xmlChar *)"option", (xmlChar *)ctmp);
	g_free(ctmp);
	ctmp = g_strdup_printf(
		"%d",
		GPOINTER_TO_INT(
			g_hash_table_lookup(
				rf->hrttl,
				lookup_key(key))));
	xmlSetProp (src, (xmlChar *)"value", (xmlChar *)ctmp);
	g_free(ctmp);
	ctmp = g_strdup_printf(
		"%d",
		GPOINTER_TO_INT(
			g_hash_table_lookup(
				rf->hrttl_multiply,
				lookup_key(key))));
	xmlSetProp (src, (xmlChar *)"factor", (xmlChar *)ctmp);
	g_free(ctmp);

	xmlDocDumpMemory (doc, &xmlbuf, &n);
	xmlFreeDoc (doc);

	/* remap to glib memory */
	tmp = g_malloc (n + 1);
	memcpy (tmp, xmlbuf, n);
	tmp[n] = '\0';
	xmlFree (xmlbuf);

	return tmp;

}

void
prepare_feed(gpointer key, gpointer value, gpointer user_data)
{
	char *xmlbuf;

	xmlbuf = feed_to_xml (key);
	if (xmlbuf)
		rss_list = g_slist_append (rss_list, xmlbuf);
}

void
load_gconf_feed(void)
{
	GSList *list, *l = NULL;
	char *uid;

	list = gconf_client_get_list (rss_gconf,
		"/apps/evolution/evolution-rss/feeds",
		GCONF_VALUE_STRING, NULL);
	for (l = list; l; l = l->next) {
		uid = feeds_uid_from_xml (l->data);
		if (!uid)
			continue;

		feed_new_from_xml (l->data);

		g_free (uid);
	}
	g_slist_foreach(list, (GFunc) g_free, NULL);
	g_slist_free(list);
}

void
save_gconf_feed(void)
{

	g_hash_table_foreach(rf->hrname, prepare_feed, NULL);

	gconf_client_set_list (
		rss_gconf,
		"/apps/evolution/evolution-rss/feeds",
		GCONF_VALUE_STRING,
		rss_list,
		NULL);

	while (rss_list) {
		g_free (rss_list->data);
		rss_list = g_slist_remove (rss_list, rss_list->data);
	}

	gconf_client_suggest_sync (rss_gconf, NULL);
}


void
migrate_old_config(gchar *feed_file)
{
	FILE *ffile;
	gchar rfeed[512];
	char **str;
	gpointer key;

	memset(rfeed, 0, 512);

	if ((ffile = fopen(feed_file, "r"))) {
		while (fgets(rfeed, 511, ffile) != NULL) {
			str = g_strsplit(rfeed, "--", 0);
			key = gen_md5(str[1]);
			g_hash_table_insert(rf->hrname,
				g_strdup(str[0]),
				g_strdup(key));
			g_hash_table_insert(rf->hrname_r,
				g_strdup(key),
				g_strdup(str[0]));
			g_hash_table_insert(rf->hr,
				g_strdup(key),
				g_strstrip(str[1]));
			if (NULL != str[4]) {
				g_hash_table_insert(rf->hrh,
					g_strdup(key),
					GINT_TO_POINTER(atoi(g_strstrip(str[4]))));
				g_hash_table_insert(rf->hrt,
					g_strdup(key),
					g_strdup(str[3]));
				g_hash_table_insert(rf->hre,
					g_strdup(key),
					GINT_TO_POINTER(atoi(str[2])));
			} else {
				if (NULL != str[2]) {    // 0.0.1 -> 0.0.2
					g_hash_table_insert(rf->hrh,
						g_strdup(key),
						(gpointer)0);
					g_hash_table_insert(rf->hrt,
						g_strdup(key),
						g_strstrip(str[3]));
					g_hash_table_insert(rf->hre,
						g_strdup(key),
						GINT_TO_POINTER(atoi(str[2])));
				} else {
					g_hash_table_insert(rf->hrh,
						g_strdup(key),
						(gpointer)0);
					g_hash_table_insert(rf->hrt,
						g_strdup(key),
						g_strdup("RSS"));
					g_hash_table_insert(rf->hre,
						g_strdup(key),
						(gpointer)1);
				}
			}
			g_free(key);
		}
		fclose(ffile);
		save_gconf_feed();
		unlink(feed_file);
	}
}

void
read_feeds(rssfeed *rf)
{
	gchar *feed_dir = rss_component_peek_base_directory();
	gchar *feed_file;

	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);
	feed_file = g_strdup_printf("%s/evolution-feeds", feed_dir);
	g_free(feed_dir);
	rf->hrname =
		g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, g_free);
	rf->hrname_r =
		g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, g_free);
	rf->hr = g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, g_free);
	rf->hre = g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, NULL);
	rf->hrt =
		g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, g_free);
	rf->hrh =
		g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, NULL);
	rf->hruser =
		g_hash_table_new_full(
			g_str_hash, g_str_equal, NULL, g_free);
	rf->hrpass =
		g_hash_table_new_full(
			g_str_hash, g_str_equal, NULL, g_free);
	rf->hrdel_feed =
		g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, NULL);
	rf->hrdel_days =
		g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, NULL);
	rf->hrdel_messages =
		g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, NULL);
	rf->hrdel_unread =
		g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, NULL);
	rf->hrdel_notpresent =
		g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, NULL);
	rf->hrupdate = g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, NULL);
	rf->hrttl = g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, NULL);
	rf->hrttl_multiply = g_hash_table_new_full(
			g_str_hash, g_str_equal, g_free, NULL);

	if (g_file_test(feed_file, G_FILE_TEST_EXISTS))
		migrate_old_config(feed_file);
	else
		load_gconf_feed();

	g_free(feed_file);
}

gchar *
get_main_folder(void)
{
	gchar mf[512];
	gchar *feed_file;
	gchar *feed_dir = rss_component_peek_base_directory();

	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);
	feed_file = g_strdup_printf("%s" G_DIR_SEPARATOR_S "main_folder", feed_dir);
	g_free(feed_dir);
	if (g_file_test(feed_file, G_FILE_TEST_EXISTS)) {
		FILE *f = fopen(feed_file, "r");
		if (f && fgets(mf, 511, f) != NULL) {
			fclose(f);
			g_free(feed_file);
			return g_strdup(mf);
		}
		fclose(f);
	}
	g_free(feed_file);
	return g_strdup(DEFAULT_FEEDS_FOLDER);
}

void
get_feed_folders(void)
{
	gchar tmp1[512], tmp2[512];
	gchar *feed_dir, *feed_file;

	rf->feed_folders = g_hash_table_new_full(
		g_str_hash,
		g_str_equal,
		g_free, g_free);
	rf->reversed_feed_folders = g_hash_table_new_full(
		g_str_hash,
		g_str_equal,
		g_free, g_free);
	feed_dir = rss_component_peek_base_directory();
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);
	feed_file = g_strdup_printf("%s" G_DIR_SEPARATOR_S "feed_folders", feed_dir);
	g_free(feed_dir);
	if (g_file_test(feed_file, G_FILE_TEST_EXISTS)) {
		FILE *f = fopen(feed_file, "r");
		while (!feof(f)) {
			fgets(tmp1, 512, f);
			fgets(tmp2, 512, f);
			g_hash_table_insert(
				rf->feed_folders,
				g_strdup(g_strstrip(tmp1)),
				g_strdup(g_strstrip(tmp2)));
		}
		fclose(f);
	}
	g_free(feed_file);
	g_hash_table_foreach(
		rf->feed_folders,
		(GHFunc)populate_reversed,
		rf->reversed_feed_folders);
}

//migrates old feed data files from crc naming
//to md5 naming while preserving content
//
//this will be obsoleted over a release or two

void
migrate_crc_md5(const char *name, gchar *url)
{
	gchar *crc = gen_crc(name);
	gchar *crc2 = gen_crc(url);
	gchar *md5, *md5_name, *feed_dir, *feed_name;

	md5 = gen_md5(url);
	feed_dir = rss_component_peek_base_directory();
	if (!g_file_test(feed_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (feed_dir, 0755);

	md5_name = g_build_path(G_DIR_SEPARATOR_S, feed_dir, md5, NULL);
	feed_name = g_build_path(G_DIR_SEPARATOR_S, feed_dir, crc, NULL);
	g_free(crc);
	g_free(md5);

	if (g_file_test(feed_name, G_FILE_TEST_EXISTS)) {
		FILE *fr = fopen(feed_name, "r");
		FILE *fw = fopen(md5_name, "a+");
		gchar rfeed[513];
		memset(rfeed, 0, 512);
		if (fr && fw) {
			while (fgets(rfeed, 511, fr) != NULL) {
				(void)fseek(fw, 0L, SEEK_SET);
				fwrite(rfeed, strlen(rfeed), 1, fw);
			}
			unlink(feed_name);
		}
		if (fr) fclose(fr);
		if (fw)	fclose(fw);
	}
	g_free(feed_name);
	feed_name = g_build_path(G_DIR_SEPARATOR_S, feed_dir, crc2, NULL);
	g_free(crc2);
	if (g_file_test(feed_name, G_FILE_TEST_EXISTS)) {
		FILE *fr = fopen(feed_name, "r");
		FILE *fw = fopen(md5_name, "a+");
		gchar rfeed[513];
		memset(rfeed, 0, 512);
		if (fr && fw) {
			while (fgets(rfeed, 511, fr) != NULL) {
				(void)fseek(fw, 0L, SEEK_SET);
				fwrite(rfeed, strlen(rfeed), 1, fw);
			}
			unlink(feed_name);
		}
		if (fr) fclose(fr);
		if (fw)	fclose(fw);
	}

	g_free(feed_name);
	g_free(feed_dir);
	g_free(md5_name);
}

