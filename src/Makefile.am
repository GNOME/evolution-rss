
DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install

INCLUDES =							\
	-I$(top_srcdir)						\
	$(RENDER_CFLAGS)					\
	$(DBUS_CFLAGS)						\
	$(LIBSOUP_GNOME_CFLAGS)					\
	$(GTKHTML_EDITOR_CFLAGS)				\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""		\
	-DLOCALEDIR="\"$(LOCALEDIR)\""				\
	-DEVOLUTION_VERSION_STRING=\"$(EVOLUTION_EXEC_VERSION)\"\
	-DEVOLUTION_VERSION=$(evolution_version_int)		\
	-DDATASERVER_VERSION=$(DATASERVER_VERSION)		\
	-DEVOLUTION_UIDIR=\""$(uidir)"\"			\
	-DEVOLUTION_ICONDIR=\""$(ICON_DIR)"\"			\
	-DGTK_VERSION="$(GTK_VERSION)"				\
	-DLIBSOUP_VERSION="$(SOUP_VERSION)"			\
	-DWEBKIT_VERSION="$(WEBKIT_VERSION)"			\
	-DRENDER=\""$(RENDER)"\"				\
	-DRENDER_N=$(RENDER_N)					\
	-DHAVE_DBUS=$(DBD)					\
	-DDBUS_API_SUBJECT_TO_CHANGE				\
	$(EVOLUTION_RSS_EPLUGIN_CFLAGS)
#	$(WARNING_FLAGS)

if ! HAVE_XPCOM_GLUE
INCLUDES += -DGECKO_HOME=\""$(GECKO_HOME)"\"
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


SOURCES =	fetch.c			\
		file-gio.c		\
		misc.c			\
		network-soup.c		\
		notification.c		\
		parser.c		\
		rss.c			\
		rss-cache.c		\
		rss-config.c		\
		rss-config-factory.c	\
		rss-image.c		\
		rss-icon-factory.c
if HAVE_GECKO
SOURCES+=	gecko-utils.cpp
endif
if HAVE_DBUS
SOURCES+=	dbus.c
endif

liborg_gnome_evolution_rss_la_SOURCES = $(SOURCES)
liborg_gnome_evolution_rss_la_LIBADD = $(WEBKIT_LIBS) $(GECKO_LIBS) $(LIBSOUP_LIBS) $(GTKHTML_EDITOR_LIBS) $(DBUS_LIBS) $(NO_UNDEFINED_LIBS)
liborg_gnome_evolution_rss_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)
#$(GECKO_RPATH)

evolution_import_rss_SOURCES = evolution-import-rss.c
evolution_import_rss_LDFLAGS = $(DBUS_LIBS)


schemadir	= $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = evolution-rss.schemas.in.in
schema_DATA	= $(schema_in_files:.schemas.in.in=.schemas)
%.schemas.in: %.schemas.in.in
	sed -e "s#\@bindir\@#@bindir@#g" $< > $@

server_in_files = GNOME_Evolution_RSS.server.in.in

if HAVE_BONOBO
server_DATA = $(server_in_files:.server.in.in=_$(EVOLUTION_EXEC_VERSION).server)
	touch $(top_srcdir)/po/POTFILES.skip
	sed -i -e 's!^src\/GNOME_Evolution_RSS.server.in.in!!g' $(top_srcdir)/po/POTFILES.skip
	sed -i -e 's!^#src\/GNOME_Evolution_RSS.server.in.in!!g' $(top_srcdir)/po/POTFILES.in
	echo 'src/GNOME_Evolution_RSS.server.in.in' >> $(top_srcdir)/po/POTFILES.in
else
	sed -i -e 's!^#src\/GNOME_Evolution_RSS.server.in.in!!g' $(top_srcdir)/po/POTFILES.in
	sed -i -e 's!^src\/GNOME_Evolution_RSS.server.in.in!!g' $(top_srcdir)/po/POTFILES.in
	echo 'src/GNOME_Evolution_RSS.server.in.in' >> $(top_srcdir)/po/POTFILES.skip
endif

@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@
@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL

install-data-local:
	rm -f $(PLUGIN_INSTALL_DIR)/*gnome-cooly*
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(schema_DATA)

uninstall-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-uninstall-rule $(schema_DATA)
	$(GCONFTOOL) -u /apps/evolution/evolution-rss/remove_folder
	$(GCONFTOOL) -u /apps/evolution/evolution-rss/pass_proxy
	$(GCONFTOOL) -u /apps/evolution/evolution-rss/startup_check
	$(GCONFTOOL) -u /apps/evolution/evolution-rss/rep_check
	$(GCONFTOOL) -u /apps/evolution/evolution-rss/rep_check_timeout
	$(GCONFTOOL) -u /apps/evolution/evolution-rss/auth_proxy
	$(GCONFTOOL) -u /apps/evolution/evolution-rss/host_proxy
	$(GCONFTOOL) -u /apps/evolution/evolution-rss/html_render
	$(GCONFTOOL) -u /apps/evolution/evolution-rss/use_proxy
	$(GCONFTOOL) -u /apps/evolution/evolution-rss/user_proxy
	$(GCONFTOOL) -u /apps/evolution/evolution-rss/port_proxy
	$(GCONFTOOL) -u /apps/evolution/evolution-rss/display_summary
	$(GCONFTOOL) -u /gnome/url-handlers/feed/command
	$(GCONFTOOL) -u /gnome/url-handlers/feed/enabled
	$(GCONFTOOL) -u /gnome/url-handlers/feed/need-terminal

endif


DISTCLEANFILES = $(schema_DATA)

ui_DATA =                    \
	rss-main.ui		\
	rss-html-rendering.ui

BUILT_SOURCES	= $(server_DATA)
CLEANFILES	= $(BUILT_SOURCES)	\
	org-gnome-evolution-rss.eplug	\
	org-gnome-evolution-rss.error   \
	GNOME_Evolution_RSS.server.in	\
	evolution-rss.schemas		\
	evolution-rss.schemas.in	\
	$(schema_DATA)

EXTRA_DIST =					\
	$(ui_DATA)				\
	$(server_in_files)			\
	$(schema_in_files)			\
	rss.h network.h				\
	network-soup.c				\
	network-soup.h				\
	misc.c					\
	misc.h					\
	debug.h					\
	dbus.c					\
	dbus.h					\
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
	rss-icon-factory.c			\
	rss-icon-factory.h			\
	rss-image.c				\
	rss-image.h				\
	strptime.c				\
	evolution-import-rss.c			\
	evolution-rss.schemas.in.in		\
	org-gnome-evolution-rss.eplug.xml	\
	org-gnome-evolution-rss.error.xml	\
	org-gnome-evolution-rss.xml


if HAVE_DBUS
bin_PROGRAMS = \
	evolution-import-rss
DBD=1
else
DBD=0
endif


-include $(top_srcdir)/git.mk
