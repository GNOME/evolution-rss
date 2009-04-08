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

#include <rss-icon-factory.h>

#include <rss.h>

typedef struct {
        char *stock_id;
        char *icon;
} RssStockIcon;

static RssStockIcon stock_icons [] = {
        { RSS_TEXT_HTML, RSS_TEXT_HTML_FILE },
        { RSS_TEXT_GENERIC, RSS_TEXT_GENERIC_FILE },
        { RSS_MAIN, RSS_MAIN_FILE }
};

void
rss_build_stock_images(void)
{
        GtkIconFactory *factory;
        GtkIconSource *source;
	int i;

        source = gtk_icon_source_new();
        factory = gtk_icon_factory_new();
        gtk_icon_factory_add_default(factory);

	for (i = 0; i < G_N_ELEMENTS (stock_icons); i++) {
		GtkIconSet *set;
		gchar *iconfile = g_build_filename (EVOLUTION_ICONDIR,
                                            stock_icons[i].icon,
                                            NULL);

		gtk_icon_source_set_filename(source, iconfile);
		g_free(iconfile);

		set = gtk_icon_set_new();
		gtk_icon_set_add_source(set, source);
		gtk_icon_factory_add(factory, stock_icons[i].stock_id, set);
		gtk_icon_set_unref(set);
	}
        gtk_icon_source_free(source);
}

