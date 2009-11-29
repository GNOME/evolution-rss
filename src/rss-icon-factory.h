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

#include <e-util/e-icon-factory.h>

#define RSS_TEXT_HTML "rss-text-html"
#define RSS_TEXT_HTML_FILE "rss-text-html.png"

#define RSS_TEXT_GENERIC "rss-text-generic"
#define RSS_TEXT_GENERIC_FILE "rss-text-x-generic.png"

#define RSS_MAIN "rss-main"
#define RSS_MAIN_FILE "rss-24.png"

GdkPixbuf *rss_build_icon(const gchar *icon_name,
			GtkIconSize icon_size);
void rss_build_stock_images(void);
void init_rss_builtin_images(void);

