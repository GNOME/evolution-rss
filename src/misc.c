/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007  Lucian Langa <cooly@mips.edu.ms> 
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

int
getNumericConfValue(gpointer a)
{
 	return 1;
}
 
int
on_next_unread_item_activate(gpointer a)
{
 	return 1;
}
 
static void
print_hash(gpointer key, gpointer value, gpointer user_data)
{
 	g_print("key:%s, value:%s\n", key, value);
}
 
static void
free_hash(gpointer key, gpointer value, gpointer user_data)
{
 	g_print("FREE - key:%p, value:%p\n", key, value);
 //	xmlFreeDoc(key);
}
 
//prefixes uri with http:// if it's misssing
//resulting text should be freed when no longer needed
gchar *
sanitize_url(gchar *text)
{
 	if (!strstr (text, "http://") && !strstr (text, "https://"))
 		return g_strconcat("http://", text, NULL);
 	else
 		return g_strdup(text);
}

//evolution folder must not contain certain chars
//for instance "..." at the start of the string
//or "/" anywhere in the string
gchar *
sanitize_folder(gchar *text)
{
 	g_return_val_if_fail( uri != NULL, NULL);
	//first remove / character
	char c = "-";
	char *tmp = g_strdup(text);
	g_strdelimit(tmp, "/", '|');
	GString *str = g_string_new(NULL);
        gchar *string;
        const unsigned char *s = (const unsigned char *)tmp;
	g_string_append(str, tmp);
        guint len = strlen(tmp);
        while (*s == '.' && len)
        {
                str = g_string_erase (str, 0, 1);
		s = str->str;
             	len--;
        }
        g_string_append_c(str, 0);
        string = str->str;
        g_string_free(str, 0);
	g_free(tmp);
        return string;
}
 
static gchar *
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
get_server_from_uri(gchar *uri)
{
 	g_return_val_if_fail( uri != NULL, NULL);
 
 	gchar **str = g_strsplit(uri, "://", 2);
         gchar **str2 = g_strsplit(str[1], "/", 2);
         gchar *server = g_strdup_printf("%s://%s", str[0], str2[0]);
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
        while (*s != 0 || len)
        {
             if (*s == 0x3f)
             {
                   g_string_append(str, "%3F");
                   *s++;
             }
             else
                   g_string_append_c (str, *s++);
             len--;
        }
        g_string_append_c(str, 0);
 	string = str->str;
 	g_string_free(str, 0);	
 	return string;
} 

static gchar *
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

                        if (g_ascii_strncasecmp (iterator, "&amp;", 5) == 0)
                        {
                                g_string_append_c (result, '&');
                                jump = 5;
                        }
                        else if (g_ascii_strncasecmp (iterator, "&lt;", 4) == 0)
                        {
                                g_string_append_c (result, '<');
                                jump = 4;
                        }
                        else if (g_ascii_strncasecmp (iterator, "&gt;", 4) == 0)
                        {
                                g_string_append_c (result, '>');
                                jump = 4;
                        }
                        else if (g_ascii_strncasecmp (iterator, "&quot;", 6) == 0)
                        {
                                g_string_append_c (result, '\"');
                                jump = 6;
                        }
                        for (i = jump - 1; i > 0; i--)
                        {
                                iterator++;
                                if (*iterator == '\0')
                                        break;
                        }
                }
                else
                {
                        g_string_append_c (result, *iterator);
                }
        }
        temp = result->str;
        g_string_free (result, FALSE);
        return temp;
}

u_int32_t
gen_crc(const char *msg)
{
         register unsigned long crc, poly;
         u_int32_t crc_tab[256];
         int i,j;
 
         poly = 0xEDB88320L;
         for (i = 0; i < 256; i++)
         {
                 crc = i;
                 for (j = 8; j > 0; j--)
                 {
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
    return (crc ^ 0xFFFFFFFF);
}
 
static char *
gen_md5(gchar *buffer)
{
         unsigned char md5sum[16], res[17], *f;
         int i;
         const char tohex[16] = "0123456789abcdef";
 
         md5_get_digest (buffer, strlen(buffer), md5sum);
  	for (i=0, f = res; i<16;i++)
 	{
                unsigned int c = md5sum[i];
                *f++ = tohex[c & 0xf];
         }
 	*f++ = 0;
         return g_strdup(res);
}
 
#endif

