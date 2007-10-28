AC_INIT([evolution-rss], [0.0.6], http://mips.edu.ms/bugs/bugs.html)
AC_CONFIG_SRCDIR(src)

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_MAINTAINER_MODE

dnl make sure we keep ACLOCAL_FLAGS around for maintainer builds to work
AC_SUBST(ACLOCAL_AMFLAGS, "\${ACLOCAL_FLAGS}")

AC_PROG_INTLTOOL([0.21])

AC_PROG_CC
AC_ISC_POSIX
AC_HEADER_STDC
AM_DISABLE_STATIC
AM_PROG_LIBTOOL

dnl GNOME_COMPILE_WARNINGS

AC_MSG_CHECKING(Evolution version)
dnl is this a reasonable thing to do ?
EVOLUTION_VERSION=`pkg-config --modversion evolution-shell 2>/dev/null`
if test -n "$EVOLUTION_VERSION"; then
	EVOLUTION_BASE_VERSION=$EVOLUTON_VERSION;
	EVOLUTION_BASE_VERSION_S=""
	EVOLUTION_EXEC_VERSION=`pkg-config --variable=execversion evolution-shell 2>/dev/null`
	if test -n "$EVOLUTION_EXEC_VERSION"; then
		break;
	else
		dnl we need major minor here
		EVOLUTION_EXEC_VERSION=$EVOLUTION_BASE_VERSION
		break;
	fi
else
	evo_versions='12 11 10 8 6 4'
	for i in $evo_versions; do
		EVOLUTION_VERSION=`pkg-config --modversion evolution-shell-2.$i 2>/dev/null`
		if test -n "$EVOLUTION_VERSION"; then
			EVOLUTION_BASE_VERSION=2.$i
			EVOLUTION_BASE_VERSION_S="-"$EVOLUTION_BASE_VERSION
			dnl this might be required for devel version
			EVOLUTION_EXEC_VERSION=`pkg-config --variable=execversion evolution-shell-2.$i 2>/dev/null`
			if test -n "$EVOLUTION_EXEC_VERSION"; then
				break;
			else
				EVOLUTION_EXEC_VERSION=$EVOLUTION_BASE_VERSION
				break;
			fi
			break;
		else
			continue;
		fi
	done
	if test -z "$EVOLUTION_VERSION"; then
       		AC_MSG_ERROR(Evolution development libraries not installed)
	fi
fi
AC_SUBST(EVOLUTION_VERSION)
AC_MSG_RESULT($EVOLUTION_VERSION)
AC_SUBST(EVOLUTION_BASE_VERSION)
AC_SUBST(EVOLUTION_EXEC_VERSION)
if test "x$EVOLUTION_EXEC_VERSION" = "x2.12" || 
   test	"x$EVOLUTION_EXEC_VERSION" = "x2.11"; then
	AC_DEFINE_UNQUOTED(EVOLUTION_2_12,1, [evolution mail 2.12 present])
	AC_SUBST(EVOLUTION_2_12)
fi

dnl base packages and versions
LIBGTK_REQUIRED=2.4.0
LIBGNOME_REQUIRED=2.7.0
LIBGNOMEUI_REQUIRED=2.7.0
EVOLUTION_REQUIRED=2.4.1
LIBSOUP_REQUIRED=2.2
LIBGLADE_REQUIREED=2.0

PKG_CHECK_MODULES(EVOLUTION_RSS_EPLUGIN,
  [libgnome-2.0 >= $LIBGNOME_REQUIRED dnl
   libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED dnl
   gtk+-2.0 >= $LIBGTK_REQUIRED dnl
   libsoup-2.2 >= $LIBSOUP_REQUIRED dnl
   evolution-plugin$EVOLUTION_BASE_VERSION_S >= $EVOLUTION_REQUIRED dnl
   libglade-2.0 dnl
])

dnl Evolution plugin install directory
AC_ARG_WITH(plugin-install-dir, [  --with-plugin-install-dir=PATH path to evolution plugin directory])
if test "x$with_plugin_install_dir" = "x"; then
   	PLUGIN_INSTALL_DIR=`pkg-config --variable=plugindir evolution-plugin$EVOLUTION_BASE_VERSION_S`
   	if test "x$PLUGIN_INSTALL_DIR" = "x"; then
      		AC_MSG_ERROR(Unable to find plugin directory)
		break;
	fi
fi
AC_SUBST(PLUGIN_INSTALL_DIR)

dnl Evolution images directory
AC_ARG_WITH(icon-dir, [  --with-icon-dir=PATH path to evolution icon directory])
if test "x$with_icon_dir" = "x" ; then
   ICON_DIR=`pkg-config --variable=imagesdir evolution-shell$EVOLUTION_BASE_VERSION_S`
      if test "x$ICON_DIR" = "x"; then
            AC_MSG_ERROR(Unable to find image directory)
       fi
fi
AC_SUBST(ICON_DIR)

dnl Evolution e-error install directory
ERROR_DIR=`pkg-config --variable=errordir evolution-plugin$EVOLUTION_BASE_VERSION_S`
if test "x$ERROR_DIR" = "x"; then
   AC_MSG_ERROR(Unable to find error file directory)
fi
AC_SUBST(ERROR_DIR)

# *******************************
# gecko renderer
# *******************************


GECKO_INIT([GECKO], [have_gecko=yes], [have_gecko=no])

AC_SUBST([GECKO])
AC_SUBST([GECKO_FLAVOUR])
AC_SUBST([GECKO_INCLUDE_ROOT])
AC_SUBST([GECKO_HOME])
AC_SUBST([GECKO_PREFIX])
AC_SUBST([GECKO_EXTRA_LIBS])

case "$GECKO" in
mozilla) gecko_min_version=1.7 ;;
seamonkey) gecko_min_version=1.0 ;;
*firefox) gecko_min_version=1.0 ;;
*thunderbird) gecko_min_version=1.0 ;;
xulrunner) gecko_min_version=1.8 ;;
esac

AC_MSG_CHECKING([whether to include Gecko support])
AC_ARG_ENABLE([gecko],
        AS_HELP_STRING([--disable-gecko],[Disable Gecko (gtkmozembed) support (default: enabled)]),
        [],[enable_gecko=yes])
AC_MSG_RESULT([$enable_gecko])

if test "$enable_gecko" = "yes"; then

	if test "x$have_gecko" != "xno"; then
	PKG_CHECK_MODULES([GECKO],[$GECKO_EXTRA_PKG_DEPENDENCIES $GECKO-xpcom >= $gecko_min_version], [], [have_gecko=no])
	AC_SUBST([GECKO_CFLAGS])
	AC_SUBST([GECKO_LIBS])

		if test "x$have_gecko" != "xno"; then
		AC_DEFINE_UNQUOTED(HAVE_RENDERKIT,1, [either webkit or gtkmozemebed render engines are present])
		AC_DEFINE_UNQUOTED(HAVE_GTKMOZEMBED,1, [gtkmozembed render engine present])
		AC_SUBST(HAVE_RENDERKIT)
		AC_SUBST(HAVE_GTKMOZEMEBED)
		GECKO_RPATH="-Wl,--rpath -Wl,"$GECKO_HOME
		AC_SUBST([GECKO_RPATH])
	
			if test "x$GECKO" == "xxulrunner"; then
			AC_DEFINE_UNQUOTED(HAVE_XULRUNNER, 1, [gecko flavour is xulrunner])
			AC_SUBST(HAVE_XULRUNNER)
			fi
		fi
	fi
fi
# *******************************
# webkit renderer
# *******************************

AC_MSG_CHECKING([whether to include Webkit support])
AC_ARG_ENABLE([webkit],
        AS_HELP_STRING([--disable-webkit],[Disable Webkit (Apple's Upstream Webkit) support (default: enabled)]),
        [],[enable_webkit=yes])
AC_MSG_RESULT([$enable_webkit])

if test "$enable_webkit" = "yes"; then
PKG_CHECK_MODULES([WEBKIT], WebKitGdk, [], [have_webkit=no])
AC_SUBST([WEBKIT_CFLAGS])
AC_SUBST([WEBKIT_LIBS])

if test "x$have_webkit" != "xno"; then
	AC_DEFINE_UNQUOTED(HAVE_RENDERKIT,1, [either webkit or gecko render engines are present])
	AC_DEFINE_UNQUOTED(HAVE_WEBKIT,1, [webkit render engine present])
	AC_SUBST(HAVE_RENDERKIT)
	AC_SUBST(HAVE_WEBKIT)
fi
fi

if pkg-config --atleast-version=2.6.0 gtk+-2.0; then
    AC_DEFINE(GTK_2_6,1,[At least 2.6 present])
fi

dnl ***********
dnl GConf stuff
dnl ***********
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
AM_GCONF_SOURCE_2

dnl I18n stuff
ALL_LINGUAS="en_AU ro sv"

GETTEXT_PACKAGE=evolution-rss
AC_SUBST(GETTEXT_PACKAGE)
AM_GLIB_GNU_GETTEXT
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [definition of GETTEXT_PACKAGE])
AM_GLIB_DEFINE_LOCALEDIR(GNOMELOCALEDIR)
LOCALEDIR=$localedir
AC_SUBST(LOCALEDIR)

SOEXT='.so'
AC_SUBST(SOEXT)

EVO_SERVER_RULE=$srcdir/server.mk
AC_SUBST_FILE(EVO_SERVER_RULE)

dnl test required for bonobo server installation
dnl user might specify wrong prefix or not specify at all
AC_ARG_WITH(bonobo-servers-dir, [  --with-bonobo-servers-dir=PATH path to bonobo servers directory])
if test "x$with_bonobo_servers_dir" = "x" ; then
    BONOBO_LIBDIR=`pkg-config --variable=libdir evolution-plugin$EVOLUTION_BASE_VERSION_S`
    if test "x$BONOBO_LIBDIR" = "x"; then
       AC_MSG_ERROR(Unable to find bonobo servers file directory)
    fi
fi
AC_SUBST(BONBONO_LIBDIR)

serverdir="$BONOBO_LIBDIR/bonobo/servers"
AC_SUBST(serverdir)

privdatadir='${datadir}'/evolution/$EVOLUTION_BASE_VERSION
AC_SUBST(privdatadir)

gladedir="$privdatadir/glade"
AC_SUBST(gladedir)

# Generated files from automake
AC_CONFIG_FILES([
Makefile
evolution-rss.spec
src/Makefile
po/Makefile.in
pixmaps/Makefile
])

AC_OUTPUT

echo ""
echo "Evolution RSS plugin successfully configured"
echo ""
echo "    Installation path: $PLUGIN_INSTALL_DIR"
kit="HTML rendering   : gtkhtml "
if test "x$have_webkit" != "xno" -a "$enable_webkit" = "yes"; then
	kit=$kit"WebKit "
fi
if test "x$have_gecko" != "xno" -a "$enable_gecko" = "yes"; then
	kit=$kit"GECKO($GECKO)"
fi
	echo "    "$kit
echo ""
