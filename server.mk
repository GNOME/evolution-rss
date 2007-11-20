%.server.in: %.server.in.in
	sed -e 's|\@PLUGIN_INSTALL_DIR\@|$(PLUGIN_INSTALL_DIR)|'	\
	-e 's|\@ICON_DIR\@|$(ICON_DIR)|'	\
	-e 's|\@VERSION\@|$(EVOLUTION_EXEC_VERSION)|' 			\
	-e 's|\@EXEEXT\@|$(EXEEXT)|'				\
	-e 's|\@SOEXT\@|$(SOEXT)|' $< > $@

%_$(EVOLUTION_EXEC_VERSION).server: %.server
	mv $< $@

%.eplug.xml.in: 
	echo "<hook class=org.gnome.evolution.mail.bonobomenu:1.0> \
      <menu target=select id=org.gnome.evolution.mail.browser> \
	<ui file=+PLUGIN_INSTALL_DIR+/org-gnome-evolution-rss.xml> \
          <item verb=RSSTask type=item path=commands/RSSTask activate=org_gnome_cooly_rss_refresh> \
        </menu> \
    </hook>" > test
