/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2010  Lucian Langa <cooly@gnome.eu.org>
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

#ifndef __MISC_C_
#define __MISC_C_

#define _GNU_SOURCE
#include <stdint.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#if (DATASERVER_VERSION >= 2031001)
#include <camel/camel.h>
#else
#include <camel/camel-mime-utils.h>
#endif
#include <e-util/e-mktemp.h>


#include "rss.h"
#include "parser.h"
#include "misc.h"

extern int rss_verbose_debug;
extern rssfeed *rf;

int getNumericConfValue(gpointer a);

int
getNumericConfValue(gpointer a)
{
	return 1;
}

int on_next_unread_item_activate(gpointer a);

int
on_next_unread_item_activate(gpointer a)
{
	return 1;
}

void
print_list(gpointer data, gpointer user_data)
{
	g_print("list element:%s\n", (gchar *)data);
}

void
print_hash(gpointer key, gpointer value, gpointer user_data)
{
	g_print("key:%s, value:%s\n", (gchar *)key, (gchar *)value);
}

void
print_hash_int(gpointer key, gpointer value, gpointer user_data)
{
	g_print("key:%s, value:%d\n", (gchar *)key, GPOINTER_TO_INT(value));
}

void free_hash(gpointer key, gpointer value, gpointer user_data);

void
free_hash(gpointer key, gpointer value, gpointer user_data)
{
	g_print("FREE - key:%p, value:%p\n", (gchar *)key, (gchar *)value);
//	xmlFreeDoc(key);
}

void
print_cf(create_feed *CF)
{
	g_print("Sender: %s ", CF->sender);
	g_print("Subject: %s \n", CF->subj);
	g_print("Date: %s\n", CF->date);
	g_print("Feedid: %s\n", CF->feedid);
	g_print("==========================\n");
	g_print("Name: %s ", CF->feed_fname);
	g_print("URI: %s\n", CF->feed_uri);
	g_print("Path: %s\n", CF->full_path);
	g_print("Website: %s\n", CF->website);
	g_print("==========================\n");
	g_print("%s\n", CF->body);
	g_print("==========================\n");
	g_print("q: %s\n", CF->q);
	g_print("encl: %s\n", CF->encl);
	g_print("dcdate: %s\n", CF->dcdate);
}

gboolean
check_key_match (gpointer key, gpointer value, gpointer user_data)
{
	char *sf_href = (char *)key;
	char *int_uri = (char *)user_data;

	d("checking hay:%s for neddle:%s\n", sf_href, int_uri);

	if (!strcmp (sf_href, int_uri))
		return TRUE; /* Quit calling the callback */

	return FALSE; /* Continue calling the callback till end of table */
}

gboolean
check_if_match (gpointer key, gpointer value, gpointer user_data)
{
	char *sf_href = (char *)value;
	char *int_uri = (char *)user_data;

	d("checking hay:%s for neddle:%s\n", sf_href, int_uri);

	if (!strcmp (sf_href, int_uri))
		return TRUE; /* Quit calling the callback */

	return FALSE; /* Continue calling the callback till end of table */
}

void
dup_auth_data(gchar *origurl, gchar *url)
{
	gchar *user = g_hash_table_lookup(rf->hruser, origurl);
	gchar *pass = g_hash_table_lookup(rf->hrpass, origurl);
	d("origurl / url:%s / %s\n", origurl, url);
	d("user / pass:%s / %s\n", user, pass);
	if (user && pass) {
		g_hash_table_insert(rf->hruser, url, g_strdup(user));
		g_hash_table_insert(rf->hrpass, url, g_strdup(pass));
	}
}

gchar *
strextr(gchar *text, const gchar *substr)
{
	gchar *tmp, *string;
	GString *str;

	g_return_val_if_fail( text != NULL, NULL);

	if (substr == NULL)
		return g_strdup(text);
	//first check if string contains the substring
	if (!strstr(text, substr))
		return g_strdup(text);

	tmp = g_strdup(text);
	str = g_string_new(NULL);
	g_string_append(str, tmp);
	str = g_string_erase(str, strlen(tmp) - strlen(strstr(tmp, substr)), strlen(substr));
	string = str->str;
	g_string_free(str, 0);
	g_free(tmp);
	return string;
}

//prefixes uri with http:// if it's misssing
//resulting text should be freed when no longer needed
gchar *
sanitize_url(gchar *text)
{
	gchar *out;
	gchar *scheme;
	gchar *tmptext = g_strdup(text);
	gchar *tmp = NULL;

	if (strcasestr(text, "file://"))
		return tmptext;

	/* extract feed component (usually first) */
	if (strcasestr(text, "feed://"))
		tmptext = strextr(text, "feed://");
	else if (strcasestr(text, "feed//"))
		tmptext = strextr(text, "feed//");
	else if (strcasestr(text, "feed:"))
		tmptext = strextr(text, "feed:");

	if (strcasestr(text, "http//")) {
		tmp = tmptext;
		tmptext = strextr(tmp, "http//");
	}
	if (!strcasestr(tmptext, "http://") && !strcasestr(tmptext, "https://")) {
		gchar *safetext = g_strconcat("http://", tmptext, NULL);
		g_free(tmptext);
		tmptext=safetext;
	}

	scheme = g_uri_parse_scheme(tmptext);
	d("parsed scheme:%s\n", scheme);
	if (!scheme && !strstr (tmptext, "http://")
	&& !strstr (tmptext, "https://")) {
		out = g_filename_to_uri(tmptext, NULL, NULL);
	} else
		out = g_strdup(tmptext);

	g_free(tmptext);
	g_free(scheme);
	if (tmp) g_free(tmp);
	return out;
}

//evolution folder must not contain certain chars
//for instance "..." at the start of the string
//or "/" anywhere in the string
gchar *
sanitize_folder(gchar *text)
{
	gchar *tmp, *tmp2;

	g_return_val_if_fail( text != NULL, NULL);

	//first convert "/" character
	tmp = g_strdup(text);
	g_strdelimit(tmp, "/", '|');
	// Strip leading dots
	tmp2 = tmp;
	while (*tmp2 == '.') tmp2++;
	tmp2 = g_strdup (tmp2);
	g_free (tmp);
	return tmp2;
}

gchar *
get_url_basename(gchar *url)
{
	gchar *p;
	p = strrchr(url, '/');
	if (p)
		return p+1;
	else
		return url;
}

gchar *
get_port_from_uri(gchar *uri)
{
	gchar **str, **str2, **str3, *port;

	g_return_val_if_fail( uri != NULL, NULL);

	if (strstr(uri, "://") == NULL)
		return NULL;
	str = g_strsplit(uri, "://", 2);
	str2 = g_strsplit(str[1], "/", 2);
	str3 = g_strsplit(str2[0], ":", 2);
	port = g_strdup(str3[1]);
	g_strfreev(str);
	g_strfreev(str2);
	g_strfreev(str3);
	return port;
}

gchar *
get_server_from_uri(gchar *uri)
{
	gchar **str, **str2, *server;

	g_return_val_if_fail( uri != NULL, NULL);

	if (strstr(uri, "://") == NULL)
		return NULL;
	str = g_strsplit(uri, "://", 2);
	str2 = g_strsplit(str[1], "/", 2);
	server = g_strdup_printf("%s://%s", str[0], str2[0]);
	g_strfreev(str);
	g_strfreev(str2);
	return server;
}

gchar *
strplchr(gchar *source)
{
	GString *str = g_string_new(NULL);
	gchar *string;
	const unsigned char *s = (const unsigned char *)source;
	guint len = strlen(source);
	while (*s != 0 || len) {
		if (*s == 0x3f) {
			g_string_append(str, "%3F");
			s++;
		} else
			g_string_append_c (str, *s++);
		len--;
	}
	g_string_append_c(str, 0);
	string = str->str;
	g_string_free(str, 0);
	return string;
}

gchar *
markup_decode (gchar *str)
{
	char *iterator, *temp;
	int cnt = 0;
	GString *result = g_string_new (NULL);

	g_return_val_if_fail (str != NULL, NULL);

	iterator = str;

	for (cnt = 0, iterator = str;
	cnt <= (int)(strlen (str));
	cnt++, iterator++) {
		if (*iterator == '&') {
			int jump = 0;
			int i;

			if (g_ascii_strncasecmp (iterator, "&amp;", 5) == 0) {
				g_string_append_c (result, '&');
				jump = 5;
			} else if (g_ascii_strncasecmp (iterator, "&lt;", 4) == 0) {
				g_string_append_c (result, '<');
				jump = 4;
			} else if (g_ascii_strncasecmp (iterator, "&gt;", 4) == 0) {
				g_string_append_c (result, '>');
				jump = 4;
			} else if (g_ascii_strncasecmp (iterator, "&quot;", 6) == 0) {
				g_string_append_c (result, '\"');
				jump = 6;
			}
			for (i = jump - 1; i > 0; i--) {
				iterator++;
				if (*iterator == '\0')
					break;
			}
		} else {
			g_string_append_c (result, *iterator);
		}
	}
	temp = result->str;
	g_string_free (result, FALSE);
	return temp;
}

gchar *
gen_crc(const char *msg)
{
	register unsigned long crc, poly;
	uint32_t crc_tab[256];
	int i,j;

	poly = 0xEDB88320L;
	for (i = 0; i < 256; i++) {
		crc = i;
		for (j = 8; j > 0; j--) {
			if (crc & 1)
				crc = (crc >> 1) ^ poly;
			else
				crc >>= 1;
		}
		crc_tab[i] = crc;
	}

	crc = 0xFFFFFFFF;
	for (i = 0; i < strlen(msg); i++)
		crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_tab[(crc ^ *msg++) & 0xFF];
	return g_strdup_printf("%x", (unsigned int)(crc ^ 0xFFFFFFFF));
}

gchar *
gen_md5(gchar *buffer)
{
	unsigned char res[17], *f;
	guint8 *md5sum;
	gsize length;
	gchar *result;
	int i;
	const char tohex[16] = "0123456789abcdef";
	GChecksum *checksum;

	length = g_checksum_type_get_length (G_CHECKSUM_MD5);
	md5sum = g_alloca (length);

	checksum = g_checksum_new (G_CHECKSUM_MD5);
	g_checksum_update (checksum, (guchar *) buffer, -1);
	g_checksum_get_digest (checksum, md5sum, &length);
	g_checksum_free (checksum);

	for (i=0, f = res; i<length;i++) {
		unsigned int c = md5sum[i];
		*f++ = tohex[c & 0xf];
	}
	*f++ = 0;
	result = g_strdup((gchar *)res);
	return result;
}

void
header_decode_lwsp(const char **in)
{
	const char *inptr = *in;
	char c;


	while ((camel_mime_is_lwsp(*inptr) || *inptr =='(') && *inptr != '\0') {
		while (camel_mime_is_lwsp(*inptr) && *inptr != '\0') {
			inptr++;
		}

		/* check for comments */
		if (*inptr == '(') {
			int depth = 1;
			inptr++;
			while (depth && (c=*inptr) && *inptr != '\0') {
				if (c=='\\' && inptr[1]) {
					inptr++;
				} else if (c=='(') {
					depth++;
				} else if (c==')') {
					depth--;
				}
				inptr++;
			}
		}
	}
	*in = inptr;
}

char *
decode_token (const char **in)
{
	const char *inptr = *in;
	const char *start;

	header_decode_lwsp (&inptr);
	start = inptr;
	while (camel_mime_is_ttoken (*inptr))
		inptr++;
	if (inptr > start) {
		*in = inptr;
		return g_strndup (start, inptr - start);
	} else {
		return NULL;
	}
}

gchar *extract_main_folder(gchar *folder)
{
	gchar *main_folder = lookup_main_folder();
	gchar *base = g_strdup_printf("%s/", main_folder);
	gchar **nnew;
	gchar *tmp;
	if ((nnew = g_strsplit(folder, base, 0))) {
		g_free(base);
		tmp = g_strdup(nnew[1]);
		g_strfreev(nnew);
		return tmp;
	}
	else
		return NULL;
}

/* hrm, is there a library for this shit? */
struct {
	const char *name;
	int offset;
} tz_offsets [] = {
	{ "UT", 0 },
	{ "GMT", 0 },
	{ "EST", -500 },        /* these are all US timezones.  bloody yanks */
	{ "EDT", -400 },
	{ "CST", -600 },
	{ "CDT", -500 },
	{ "MST", -700 },
	{ "MDT", -600 },
	{ "PST", -800 },
	{ "PDT", -700 },
	{ "Z", 0 },
	{ "A", -100 },
	{ "M", -1200 },
	{ "N", 100 },
	{ "Y", 1200 },
};

static const char tz_months [][4] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char tz_days [][4] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

gboolean
is_rfc822(char *in)
{
	const char *inptr = in;
	struct tm tm;
	guint i;
	gchar *day, *monthname;
	gboolean foundmonth;

	header_decode_lwsp (&inptr);
	day =  decode_token(&inptr);
	if (day) {
		g_free (day);
		header_decode_lwsp (&inptr);
		if (*inptr == ',')
			inptr++;
		else
			goto notrfc;
	}
	tm.tm_mday = camel_header_decode_int(&inptr);
	if (tm.tm_mday == 0)
		goto notrfc;

	monthname = decode_token(&inptr);
	foundmonth = FALSE;
	if (monthname) {
		for (i=0;i<sizeof(tz_months)/sizeof(tz_months[0]);i++) {
		if (!g_ascii_strcasecmp(tz_months[i], monthname)) {
				tm.tm_mon = i;
				foundmonth = TRUE;
				break;
			}
		}
		g_free(monthname);
	}
	if (!foundmonth)
		goto notrfc;

	return 1;

notrfc:	return 0;
}

gchar *
encode_rfc2047(gchar *str)
{
	gchar *tmp = decode_entities(str);
	gchar *rfctmp = camel_header_encode_string((unsigned char*)tmp);
	g_free(tmp);
	return (gchar *)rfctmp;
}

/* this whole process is not optimal
 * but for the time being it will have to do
 */
void
feed_remove_status_line(gchar *file_name, gchar *needle)
{
	gchar rfeed[513];
	FILE *fr, *fw = NULL;
	gchar *tmpneedle, *port, *tp;
	gchar *tmp = e_mktemp("evo-rss-XXXXXX");
	if (tmp) {
		fw = fopen(tmp, "w+");
		if (!fw)
			return;
	}

	memset(rfeed, 0, 512);
	fr = fopen(file_name, "r");
	tmpneedle = NULL;
	port =  get_port_from_uri(needle);
	if (port && atoi(port) == 80) {
		tp = g_strconcat(":", port, NULL);
		g_free(port);
		tmpneedle = strextr(needle, tp);
		g_free(tp);
	} else
		tmpneedle = g_strdup(needle);

	if (fr) {
		while (fgets(rfeed, 511, fr) != NULL) {
			if (!g_strstr_len(rfeed, -1, tmpneedle)) {
				fputs(rfeed, fw);
			}
		}
		fclose(fr);
		g_unlink(file_name);
		g_rename(tmp, file_name);
	}
	fclose(fw);
	g_free(tmpneedle);
}

//check if feed already exists in feed file
//and if not add it to the feed file
gboolean
feed_is_new(gchar *file_name, gchar *needle)
{
	gchar rfeed[513];
	FILE *fr;
	int occ = 0;
	gchar *tmpneedle, *port, *tp;

	memset(rfeed, 0, 512);
	fr = fopen(file_name, "r");
	tmpneedle = NULL;
	port =  get_port_from_uri(needle);
	if (port && atoi(port) == 80) {
		tp = g_strconcat(":", port, NULL);
		g_free(port);
		tmpneedle = strextr(needle, tp);
		g_free(tp);
	} else
		tmpneedle = g_strdup(needle);

	if (fr) {
		while (fgets(rfeed, 511, fr) != NULL) {
			if (g_strstr_len(rfeed, -1, tmpneedle)) {
				occ=1;
				break;
			}
		}
		fclose(fr);
	}
	g_free(tmpneedle);
	return occ;
}

void
write_feed_status_line(gchar *file, gchar *needle)
{
	FILE *fw = fopen(file, "a+");
	if (fw) {
		fputs(g_strstrip(needle), fw);
		fputs("\n", fw);
		fclose(fw);
	}
}

#ifdef _WIN32
char *strcasestr(const char *a, const char *b)
{
	char *a2=g_ascii_strdown(a,-1), *b2=g_ascii_strdown(b,-1), *r=strstr(a2,b2);
	if(r)
		r=(char *)a+(r-a2);
	g_free(a2);
	g_free(b2);
	return r;
}
#endif

void
sanitize_path_separator(gchar *str)
{
#ifdef _WIN32
	while (*str != '\0') {
		if (G_IS_DIR_SEPARATOR(*str))
			*str = G_DIR_SEPARATOR;
		str++;
	}
#endif
}

#endif

