
#include <rss-icon-factory.h>

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

