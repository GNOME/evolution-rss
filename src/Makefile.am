
DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install

AM_CPPFLAGS =							\
	-I$(top_srcdir)						\
	$(RENDER_CFLAGS)					\
	$(LIBSOUP_GNOME_CFLAGS)					\
	$(GTKHTML_EDITOR_CFLAGS)				\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""		\
	-DLOCALEDIR="\"$(LOCALEDIR)\""				\
	-DEVOLUTION_VERSION_STRING=\"$(EVOLUTION_EXEC_VERSION)\"\
	-DEVOLUTION_VERSION=$(evolution_version_int)		\
	-DDATASERVER_VERSION=$(DATASERVER_VERSION)		\
	-DEVOLUTION_BINDIR=\""$(exec_prefix)"\"			\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"			\
	-DEVOLUTION_ICONDIR=\""$(ICON_DIR)"\"			\
	-DLIBSOUP_VERSION="$(SOUP_VERSION)"			\
	-DWEBKIT_VERSION="$(WEBKIT_VERSION)"			\
	-DRENDER=\""$(RENDER)"\"				\
	-DRENDER_N=$(RENDER_N)					\
	$(EVOLUTION_RSS_EPLUGIN_CFLAGS)

if ! HAVE_XPCOM_GLUE
AM_CPPFLAGS += -DGECKO_HOME=\""$(GECKO_HOME)"\"
endif

RSS_MENU = \
	<hook class=\"org.gnome.evolution.mail.bonobomenu:1.0\">\n		\
		<menu target=\"select\" id=\"org.gnome.evolution.mail.browser\">\n	\
	\n								\
	<ui file=\"+PLUGIN_INSTALL_DIR+/org-gnome-evolution-rss.xml\"/>\n	\
		<item verb=\"RSSTask\" type=\"item\" path=\"/commands/RSSTask\" activate=\"org_gnome_cooly_rss_refresh\"/>\n		\
		</menu>\n \
		</hook>

%.eplug.in.in: %.eplug.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@

%.eplug.in: %.eplug.in.in
	@if test $(MINOR_VERSION) -lt 11000; then		\
		sed -e "s%+RSS_MENU+%$(RSS_MENU)%" $< > $@;	\
	else							\
		sed -e "s%+RSS_MENU+%%" $< > $@;		\
	fi

%.eplug: %.eplug.in
	sed -e "s%+PLUGIN_INSTALL_DIR+%$(PLUGIN_INSTALL_DIR)%" \
	-e "s%+GETTEXT_PACKAGE+%$(GETTEXT_PACKAGE)%" \
	-e "s%+LOCALEDIR+%$(LOCALEDIR)%" \
	-e "s%+VERSION+%$(VERSION)%" \
	-e "s%+SOEXT+%$(SOEXT)%" \
	-e "s%+URL+%http://gnome.eu.org/evo/index.php/Evolution_RSS_Reader_Plugin%" \
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


SOURCES =	dbus.c			\
		fetch.c			\
		file-gio.c		\
		misc.c			\
		network-soup.c		\
		notification.c		\
		parser.c		\
		rss.c			\
		rss-cache.c		\
		rss-config.c		\
		rss-config-factory.c	\
		rss-evo-common.c	\
		rss-image.c		\
		rss-icon-factory.c	\
		rss-status-icon.c
if HAVE_GECKO
SOURCES+=	gecko-utils.cpp
endif

liborg_gnome_evolution_rss_la_SOURCES = $(SOURCES)
liborg_gnome_evolution_rss_la_LIBADD = $(WEBKIT_LIBS) $(DATASERVER_LIBS) $(GECKO_LIBS) $(LIBSOUP_LIBS) $(GTKHTML_EDITOR_LIBS) $(DBUS_LIBS) $(NO_UNDEFINED_LIBS)
liborg_gnome_evolution_rss_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

evolution_import_rss_SOURCES = evolution-import-rss.c
evolution_import_rss_LDFLAGS = $(EVOLUTION_RSS_EPLUGIN_LIBS)

module_LTLIBRARIES = evolution-module-rss.la

evolution_module_rss_la_SOURCES =		\
	rss.c					\
	fetch.c					\
	file-gio.c				\
	network-soup.c				\
	misc.c					\
	notification.c				\
	parser.c				\
	rss-config.c				\
	rss-config-factory.c			\
	rss-cache.c				\
	rss-evo-common.c			\
	rss-image.c				\
	rss-formater.c				\
	rss-status-icon.c			\
	rss-icon-factory.c			\
	dbus.c					\
	e-mail-parser-evolution-rss.c		\
	e-mail-parser-evolution-rss.h		\
	e-mail-formatter-evolution-rss.c	\
	e-mail-formatter-evolution-rss.h	\
	evolution-module-rss.c



evolution_module_rss_la_CPPFLAGS =			\
        $(AM_CPPFLAGS)					\
        -I$(top_srcdir)					\
        -DEVOLUTION_PRIVDATADIR=\""$(privdatadir)"\"	\
        -DG_LOG_DOMAIN=\"evolution-module-rss\"		\
        $(EVOLUTION_RSS_EPLUGIN_LIBS)

evolution_module_rss_la_LDFLAGS =		\
        -avoid-version -module $(NO_UNDEFINED)

@EVO_SERVER_RULE@

gsettings_SCHEMAS =                            \
	org.gnome.evolution.plugin.evolution-rss.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@

if HAVE_GSETTINGS
@GSETTINGS_RULES@
endif

convertdir = $(datadir)/GConf/gsettings
convert_DATA = evolution-rss.convert


DISTCLEANFILES = $(schema_DATA)			\
	evolution-rss.desktop.in		\
	$(gsettings_SCHEMAS:.xml)

ui_DATA =			\
	rss-main.ui		\
	rss-html-rendering.ui

CLEANFILES	= $(BUILT_SOURCES)	\
	org-gnome-evolution-rss.eplug	\
	org-gnome-evolution-rss.error   \
	$(gsettings_SCHEMAS)		\
	$(gsettings_SCHEMAS:.xml)

EXTRA_DIST =					\
	$(ui_DATA)				\
	$(convert_DATA)                         \
	$(gsettings_SCHEMAS:.xml=.xml.in)	\
	rss.h network.h				\
	network-soup.c				\
	network-soup.h				\
	misc.c					\
	misc.h					\
	debug.h					\
	dbus.c					\
	dbus.h					\
	e-mail-formatter-parser-rss.c		\
	e-mail-formatter-parser-rss.h		\
	e-mail-formatter-evolution-rss.c	\
	e-mail-formatter-evolution-rss.h	\
	fetch.c					\
	fetch.h					\
	file-gio.c				\
	file-gio.h				\
	gecko-utils.cpp				\
	gecko-utils.h				\
	notification.h				\
	parser.c				\
	parser.h				\
	rss-cache.h				\
	rss-config.c				\
	rss-config.h				\
	rss-config-factory.c			\
	rss-config-factory.h			\
	rss-evo-common.c			\
	rss-evo-common.h			\
	rss-formatter.c				\
	rss-formatter.h				\
	rss-icon-factory.c			\
	rss-icon-factory.h			\
	rss-status-icon.c			\
	rss-status-icon.h			\
	rss-image.c				\
	rss-image.h				\
	strptime.c				\
	evolution-import-rss.c			\
	evolution-rss.schemas.in.in		\
	org-gnome-evolution-rss.eplug.xml	\
	org-gnome-evolution-rss.error.xml	\
	org-gnome-evolution-rss.xml


bin_PROGRAMS = \
	evolution-import-rss


-include $(top_srcdir)/git.mk
