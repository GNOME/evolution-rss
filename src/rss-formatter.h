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

#ifndef __RSS_FORMATTER_H_
#define __RSS_FORMATTER_H_

#include <mail/e-mail-reader-utils.h>

#include "debug.h"

gchar *rss_process_feed(gchar *feed, guint len);
gchar *rss_process_website(gchar *content, gchar *website);
gboolean rss_get_current_view(void);
void rss_set_current_view(gboolean value);
gboolean rss_get_changed_view(void);
void rss_set_changed_view(gboolean value);
gboolean rss_get_is_html(gchar *feedid);
EMailDisplay *rss_get_display(void);

#endif /*__RSS_FORMATTER_H_*/

