%.server.in: %.server.in.in
	sed -e 's|\@PLUGINDIR_IN_SERVER_FILE\@|$(PLUGINDIR_IN_SERVER_FILE)|'    \
	-e 's|\@IMAGESDIR_IN_SERVER_FILE\@|$(IMAGESDIR_IN_SERVER_FILE)|'        \
	-e 's|\@VERSION\@|$(EVOLUTION_EXEC_VERSION)|'			\
	-e 's|\@EXEEXT\@|$(EXEEXT)|'				\
	-e 's|\@SOEXT\@|$(SOEXT)|' $< > $@

%_$(EVOLUTION_EXEC_VERSION).server: %.server
	mv $< $@
