SUBDIRS = pixmaps po src

ACLOCAL_AMFLAGS = -I m4

INTLTOOL =                      \
        intltool-extract.in     \
        intltool-merge.in       \
        intltool-update.in

EXTRA_DIST = \
	$(INTLTOOL)		\
	autogen.sh		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	server.mk		\
	FAQ

DISTCLEANFILES = \
  intltool-extract \
  intltool-merge \
  intltool-update

GITIGNOREFILES= 	aclocal.m4				\
			compile					\
			config.guess				\
			config.h.in				\
			ltmain.sh				\
			m4/libtool.m4				\
			m4/lt*.m4				\
			src/GNOME_Evolution_RSS_2.26.server

-include $(top_srcdir)/git.mk
