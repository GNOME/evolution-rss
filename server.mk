%.server.in: %.server.in.in
	sed -e 's|\@PLUGIN_INSTALL_DIR\@|$(PLUGIN_INSTALL_DIR)|'	\
	-e 's|\@ICON_DIR\@|$(ICON_DIR)|'	\
	-e 's|\@VERSION\@|$(EVOLUTION_EXEC_VERSION)|' 			\
	-e 's|\@EXEEXT\@|$(EXEEXT)|'				\
	-e 's|\@SOEXT\@|$(SOEXT)|' $< > $@

%_$(EVOLUTION_EXEC_VERSION).server: %.server
	mv $< $@
