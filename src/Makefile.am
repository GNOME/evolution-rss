
INCLUDES =							\
	-I$(EVOLUTION_SOURCE)					\
	-I$(top_srcdir)						\
	$(GECKO_CFLAGS)						\
	$(WEBKIT_CFLAGS)					\
	$(DBUS_CFLAGS)						\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""		\
	-DLOCALEDIR="\"$(LOCALEDIR)\""				\
	-DEVOLUTION_VERSION=\"$(EVOLUTION_EXEC_VERSION)\"    	\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"            	\
	-DEVOLUTION_ICONDIR=\""$(ICON_DIR)"\"            	\
	-DGECKO_HOME=\""$(GECKO_HOME)"\"			\
	-DRENDER=\""$(RENDER)"\"				\
	-DRENDER_N=$(RENDER_N)					\
	$(EVOLUTION_RSS_EPLUGIN_CFLAGS)



%.eplug.in: %.eplug.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@

%.eplug: %.eplug.in
	sed -e "s%+PLUGIN_INSTALL_DIR+%$(PLUGIN_INSTALL_DIR)%" \
	-e "s%+GETTEXT_PACKAGE+%$(GETTEXT_PACKAGE)%" \
	-e "s%+LOCALEDIR+%$(LOCALEDIR)%" \
	$< > $@

%.error.in: %.error.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@

%.error: %.error.in
	sed -e "s%+PLUGIN_INSTALL_DIR+%$(PLUGIN_INSTALL_DIR)%" \
	-e "s%+GETTEXT_PACKAGE+%$(GETTEXT_PACKAGE)%" \
	-e "s%+LOCALEDIR+%$(LOCALEDIR)%" \
	$< > $@

errordir = $(ERROR_DIR)
error_DATA = org-gnome-evolution-rss.error

plugindir = $(PLUGIN_INSTALL_DIR)
plugin_DATA = org-gnome-evolution-rss.eplug \
	org-gnome-evolution-rss.xml
plugin_LTLIBRARIES = liborg-gnome-evolution-rss.la

liborg_gnome_evolution_rss_la_SOURCES = rss.c
liborg_gnome_evolution_rss_la_LDFLAGS = -module -avoid-version $(WEBKIT_LIBS) $(GECKO_LIBS) $(GECKO_RPATH)

schemadir	= $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = evolution-rss.schemas.in
schema_DATA	= $(schema_in_files:.schemas.in=.schemas)

server_in_files = GNOME_Evolution_RSS.server.in.in
server_DATA = $(server_in_files:.server.in.in=_$(EVOLUTION_EXEC_VERSION).server)
@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@


@INTLTOOL_SCHEMAS_RULE@

install-data-local:
	rm -f $(PLUGIN_INSTALL_DIR)/*gnome-cooly*
        if test -z "$(DESTDIR)" ; then                                                                                          \
                for p in $(schema_DATA) ; do                                                                                    \
                        GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p;             \
                done                                                                                                            \
        fi


DISTCLEANFILES = $(schema_DATA)

glade_DATA =                    \
	rss-ui.glade

BUILT_SOURCES	= $(server_DATA)
CLEANFILES	= $(BUILT_SOURCES)	\
	org-gnome-evolution-rss.eplug	\
	org-gnome-evolution-rss.error   \
	GNOME_Evolution_RSS.server.in

EXTRA_DIST = 					\
	$(glade_DATA)				\
	$(server_in_files)			\
        $(schema_in_files)			\
	rss.h network.h				\
	network-soup.c				\
	misc.c					\
	org-gnome-evolution-rss.eplug.xml	\
	org-gnome-evolution-rss.error.xml	\
	org-gnome-evolution-rss.xml

