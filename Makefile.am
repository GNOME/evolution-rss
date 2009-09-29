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

# Build ChangeLog from GIT history
ChangeLog:
	@if test -d $(top_srcdir)/.git; then \
	GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

DISTCLEANFILES = \
  intltool-extract \
  intltool-merge \
  intltool-update

GITIGNOREFILES= 	aclocal.m4				\
			compile					\
			config.guess				\
			config.h.in				\
			config.sub				\
			ltmain.sh				\
			m4/libtool.m4				\
			m4/lt*.m4				\
			src/GNOME_Evolution_RSS_2.26.server

-include $(top_srcdir)/git.mk
