/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2009  Lucian Langa <cooly@gnome.eu.org>
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

#ifndef MISC_H
#define MISC_H 1

#include <rss.h>

void print_cf(create_feed *CF);
gchar *gen_crc(const char *msg);
gchar *gen_md5(gchar *buffer);
gchar *strplchr(gchar *source);
gchar *markup_decode (gchar *str);
gboolean check_key_match (gpointer key,
			gpointer value,
			gpointer user_data);
gboolean check_if_match (gpointer key,
			gpointer value,
			gpointer user_data);
gchar *get_server_from_uri(gchar *uri);
gchar *get_port_from_uri(gchar *uri);
gchar *get_url_basename(gchar *url);
gboolean is_rfc822(char *in);
gchar *extract_main_folder(gchar *folder);
gchar *strextr(gchar *text, const gchar *substr);
gchar *sanitize_url(gchar *text);
gchar *sanitize_folder(gchar *text);
void header_decode_lwsp(const char **in);
char *decode_token (const char **in);
gchar *encode_rfc2047(gchar *str);
void print_list(gpointer data, gpointer user_data);
void print_hash(gpointer key, gpointer value, gpointer user_data);
void print_hash_int(gpointer key, gpointer value, gpointer user_data);
gboolean feed_is_new(gchar *file_name, gchar *needle);
void feed_remove_status_line(gchar *file_name, gchar *needle);
void write_feed_status_line(gchar *file, gchar *needle);
void dup_auth_data(gchar *origurl, gchar *url);
void sanitize_path_separator(gchar *);

#endif
