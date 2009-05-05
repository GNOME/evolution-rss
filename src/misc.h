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

gchar *gen_md5(gchar *buffer);
gchar *strplchr(gchar *source);
gchar *markup_decode (gchar *str);
gboolean check_if_match (gpointer key, gpointer value, gpointer user_data);
gchar *get_server_from_uri(gchar *uri);

#endif
