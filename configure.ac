AC_INIT([evolution-rss], [0.1.3], http://bugzilla.gnome.org/enter_bug.cgi?product=evolution-rss)
AC_CONFIG_SRCDIR(src)

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_MAINTAINER_MODE

dnl make sure we keep ACLOCAL_FLAGS around for maintainer builds to work
AC_SUBST(ACLOCAL_AMFLAGS, "\${ACLOCAL_FLAGS}")

AC_PROG_INTLTOOL([0.21])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_REQUIRE_CPP
AC_PATH_PROG(GPP, g++, no)
if test "x$GPP" = "xno"; then
	AC_MSG_ERROR(C++ Compiler (g++) not installed)
fi
#AC_PROG_CXXCPP
AC_ISC_POSIX
AC_HEADER_STDC
AM_DISABLE_STATIC
AM_PROG_LIBTOOL

GNOME_COMPILE_WARNINGS

EVOLUTION_INIT

dnl locate libsoup flavour
SOUP=
PKG_CHECK_MODULES([LIBSOUP_24], libsoup-2.4, [SOUP=2.4], [SOUP=])
if test "x$SOUP" != "x2.4" -a "$evolution_exec_int" -ge "22190"; then
	AC_MSG_ERROR([libsoup-2.4 devel headers required])
else
if test "x$SOUP" != "x2.4"; then
	PKG_CHECK_MODULES([LIBSOUP_22], libsoup-2.2, [SOUP=2.2], [SOUP=]) 
fi
if test "x$SOUP" == "x"; then
	PKG_CHECK_MODULES(LIBSOUP, libsoup, [SOUP=], [])
else
SOUP="-$SOUP"
fi
fi

dnl base packages and versions
LIBGTK_REQUIRED=2.4.0
LIBGNOME_REQUIRED=2.7.0
LIBGNOMEUI_REQUIRED=2.7.0
EVOLUTION_REQUIRED=2.4.1
LIBSOUP_REQUIRED=2.2
LIBGLADE_REQUIRED=2.0

PKG_CHECK_MODULES(EVOLUTION_RSS_EPLUGIN,
  [libgnome-2.0 >= $LIBGNOME_REQUIRED dnl
   libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED dnl
   gtk+-2.0 >= $LIBGTK_REQUIRED dnl
   libsoup$SOUP >= $LIBSOUP_REQUIRED dnl
   evolution-plugin$EVOLUTION_BASE_VERSION_S >= $EVOLUTION_REQUIRED dnl
   libglade-2.0 dnl
])

dnl default renderer and fall it it in case no other render is found
RENDER=gtkhtml
RENDER_N=0

# *******************************
# gecko renderer
# *******************************

GECKO_DEFINES
AC_MSG_CHECKING([whether to include Gecko support])
AC_ARG_ENABLE([gecko],
        AS_HELP_STRING([--disable-gecko],[Disable Gecko (gtkmozembed) support (default: enabled)]),
        [],[enable_gecko=yes])
AC_MSG_RESULT([$enable_gecko])

if test "$enable_gecko" = "yes"; then
GECKO_INIT([GECKO], [have_gecko=yes], [have_gecko=no])

AC_SUBST([GECKO])
AC_SUBST([GECKO_FLAVOUR])
AC_SUBST([GECKO_INCLUDE_ROOT])
AC_SUBST([GECKO_HOME])
AC_SUBST([GECKO_PREFIX])
AC_SUBST([GECKO_EXTRA_LIBS])
AC_SUBST([GECKO_GLUE_LIBS])
AC_SUBST([GECKO_XPCOM])

case "$GECKO" in
mozilla) gecko_min_version=1.7 ;;
seamonkey) gecko_min_version=1.0 ;;
*firefox) gecko_min_version=1.0 ;;
*thunderbird) gecko_min_version=1.0 ;;
xulrunner) gecko_min_version=1.8 ;;
libxul*) gecko_min_version=1.9 ;;
*) AC_MSG_ERROR([No supported render found. Pleae install one of: mozilla-devel, firefox-devel, xulrunner-devel, libxul, webkit])
esac

if test "x$have_gecko" != "xno"; then
	if test $GECKO = libxul-embedding; then
		PKG_CHECK_MODULES([GECKO],[libxul-embedding $gecko_cv_extra_pkg_dependencies  >= $gecko_min_version])
		dnl https://bugzilla.mozilla.org/show_bug.cgi?id=442009
		GECKO_CFLAGS="$GECKO_CFLAGS $(pkg-config --cflags nspr)"
	elif test $GECKO = firefox; then
		GECKO_CFLAGS="$GECKO_CFLAGS $(pkg-config --cflags firefox-xpcom)"
    		PKG_CHECK_MODULES([GECKO],[$GECKO_EXTRA_PKG_DEPENDENCIES $GECKO-xpcom >= $gecko_min_version])
		AC_DEFINE_UNQUOTED(HAVE_BUGGY_GECKO, 1, [workarund for a bug in shutdown gecko])
		AC_SUBST(HAVE_BUGGY_GECKO)
	elif test $GECKO = libxul; then
    		PKG_CHECK_MODULES([GECKO],[$GECKO_EXTRA_PKG_DEPENDENCIES $GECKO >= $gecko_min_version])
	else
    		PKG_CHECK_MODULES([GECKO],[$GECKO_EXTRA_PKG_DEPENDENCIES $GECKO-xpcom >= $gecko_min_version])
	fi
	AC_SUBST([GECKO_CFLAGS])
	AC_SUBST([GECKO_LIBS])
	AC_DEFINE_UNQUOTED(HAVE_RENDERKIT,1, [either webkit or gtkmozemebed render engines are present])
	AC_DEFINE_UNQUOTED(HAVE_GECKO,1, [gecko render engine present])
	AC_SUBST(HAVE_RENDERKIT)
	AC_SUBST(HAVE_GECKO)
#		RENDER_CFLAGS="$GECKO_CFLAGS -I$GECKO_INCLUDE_ROOT" 
		RENDER_CFLAGS=$GECKO_CFLAGS 
		GECKO_RPATH="-Wl,--rpath -Wl,"$GECKO_LIBDIR
		GECKO_HOME=$GECKO_LIBDIR
		AC_SUBST([GECKO_RPATH])
	if test "x$GECKO" == "xxulrunner"; then
		AC_DEFINE_UNQUOTED(HAVE_XULRUNNER, 1, [gecko flavour is xulrunner])
		AC_SUBST(HAVE_XULRUNNER)
	fi
	if test "x$GECKO" == "xlibxul" -o "x$GECKO" == "xlibxul-embedding"; then
		AC_DEFINE_UNQUOTED(HAVE_GECKO_1_9, 1, [at least gecko 1.9])
		AC_SUBST(HAVE_GECKO_1_9)
	fi
#		_SAVE_CPPFLAGS="$CPPFLAGS"
#               	CPPFLAGS="$CPPFLAGS $GECKO_CFLAGS"
        AC_MSG_CHECKING([[whether we have a xpcom glue]])
        AC_COMPILE_IFELSE(
                 [AC_LANG_SOURCE(
                       [[
                                 #ifndef XPCOM_GLUE
                                 #error "no xpcom glue found"
                                 #endif]]
                       )],
                       [gecko_cv_have_xpcom_glue=yes],
                       [gecko_cv_have_xpcom_glue=no])
        AC_MSG_RESULT([$gecko_cv_have_xpcom_glue])
#               	CPPFLAGS="$_SAVE_CPPFLAGS"
		AC_ARG_WITH([primary-render],
        		AS_HELP_STRING([--with-primary-render],[Forces Gecko to be the primary html renderer  @<:@=gecko|webkit@:>@ (default:gtkHTML)]),
        			[RENDER=$with_primary_render], [RENDER=gtkhtml])
		if test "$RENDER" != "gtkhtml" -a "x$RENDER" == "xgecko"; then
			AC_MSG_NOTICE([$RENDER set as primary HTML render])
			RENDER_N=2
		else
			dnl fall back to gtkhtml if selection is not valid
			RENDER=gtkhtml
			RENDER_N=0
		fi
	fi
fi
AM_CONDITIONAL([HAVE_GECKO], [test "x$have_gecko" = "xyes"])
AM_CONDITIONAL([HAVE_XPCOM_GLUE], [test "$gecko_cv_have_xpcom_glue" = "yes"])

# *******************************
# webkit renderer
# *******************************

AC_MSG_CHECKING([whether to include Webkit support])
AC_ARG_ENABLE([webkit],
        AS_HELP_STRING([--disable-webkit],[Disable Webkit (Apple's Upstream Webkit) support (default: enabled)]),
        [],[enable_webkit=yes])
AC_MSG_RESULT([$enable_webkit])

if test "$enable_webkit" = "yes"; then
PKG_CHECK_MODULES([WEBKIT], webkit-1.0, [have_webkit=yes], [have_webkit=no])
if test "x$have_webkit" == "xno"; then
	PKG_CHECK_MODULES([WebKitGtk], WebKitGtk, [have_webkit=yes], [have_webkit=no])
	AC_DEFINE_UNQUOTED(WEBKIT_UNSTD, 1, [unstandard webkit installation])
	WEBKIT_CFLAGS=$WebKitGtk_CFLAGS
	WEBKIT_LIBS=$WebKitGtk_LIBS
fi
AC_SUBST([WEBKIT_CFLAGS])
AC_SUBST([WEBKIT_LIBS])
AC_SUBST([WEBKIT_UNSTD])

if test "x$have_webkit" != "xno"; then
	AC_DEFINE_UNQUOTED(HAVE_RENDERKIT,1, [either webkit or gecko render engines are present])
	AC_DEFINE_UNQUOTED(HAVE_WEBKIT,1, [webkit render engine present])
	AC_SUBST(HAVE_RENDERKIT)
	AC_SUBST(HAVE_WEBKIT)
	RENDER_CFLAGS="$RENDER_CFLAGS $WEBKIT_CFLAGS" 
	if test "x$RENDER" != "xgecko"; then
		AC_ARG_WITH([primary-render],
        		AS_HELP_STRING([--with-primary-render],[Forces Gecko to be the primary html renderer  @<:@=gecko|webkit@:>@ (default:gtkHTML)]),
        			[RENDER=$with_primary_render], [RENDER=gtkhtml])
		if test "$RENDER" != "gtkhtml" -a "x$RENDER" == "xwebkit"; then
			AC_MSG_NOTICE([$RENDER set as primary HTML render])
			RENDER_N=1
		else
			dnl fall back to gtkhtml if selection is not valid
			RENDER=gtkhtml
			RENDER_N=0
		fi
	fi
fi
fi

AC_SUBST(RENDER_CFLAGS)
AC_SUBST(RENDER)
AC_SUBST(RENDER_N)

# check for ellipsize
GTK_VERSION=`pkg-config --modversion gtk+-2.0` 
GTK_VERSION=`echo $GTK_VERSION|$AWK -F . '{print $1 * 1000000 + $2 * 1000 + $3}'`
AC_SUBST(GTK_VERSION)

dnl ***********
dnl GConf stuff
dnl ***********
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
AM_GCONF_SOURCE_2

dnl ********************
dnl extract soup version
dnl ********************
SOUP_VERSION=`pkg-config --modversion libsoup$SOUP` 
SOUP_VERSION=`echo $SOUP_VERSION|$AWK -F . '{print $1 * 1000000 + $2 * 1000 + $3}'`
AC_SUBST(SOUP_VERSION)

dnl ***********
dnl D-BUS stuff
dnl ***********
have_dbus=no
AC_ARG_ENABLE([DBUS],
        AS_HELP_STRING([--disable-dbus],[Disable D-BUS support (default: enabled)]),
        [])

if test "x$enable_dbus" != "xno"; then
	PKG_CHECK_MODULES(DBUS, dbus-glib-1, [have_dbus=yes], [have_dbus=no])
	AC_SUBST(DBUS_CFLAGS)
	AC_SUBST(DBUS_LIBS)
fi
AM_CONDITIONAL([HAVE_DBUS], [test x$have_dbus = xyes])

dnl I18n stuff
AC_PATH_PROG(GETTEXT, gettext, no)
if test "x$GETTEXT" = "xno"; then
	AC_MSG_ERROR(Package gettext not installed)
fi
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

serverdir="$BONOBO_LIBDIR/bonobo/servers"
AC_SUBST(serverdir)

privdatadir='${datadir}'/evolution/$EVOLUTION_EXEC_VERSION
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

kits="gtkHTML"
if test "x$have_webkit" != "xno" -a "$enable_webkit" = "yes"; then
        kits=$kits", WebKit"
fi
# rename libxul to xulrunner to prevent confusion on debian
if test "x$gecko_cv_gecko" == "xlibxul"; then
	gecko_cv_gecko="xulrunner"
fi
if test "x$have_gecko" != "xno" -a "$enable_gecko" = "yes"; then
kits="$kits, $gecko_cv_gecko (gecko engine version $gecko_cv_gecko_version)"
fi

if test "x$have_dbus" == "xyes"; then
	have_dbus_build="yes"
	import_feeds="evolution-import-rss"
else
	have_dbus_build="evolution-import-rss module will not be built"
	import_feeds="no"
fi

AC_OUTPUT
echo "
	Evolution RSS plugin successfully configured

	Source code location	: $srcdir
	Installation path	: $PLUGIN_INSTALL_DIR
	Prefix			: $prefix
	HTML renders		: $kits
	Primary HTML render	: $RENDER
	D-BUS support		: $have_dbus_build
	Import feeds		: $import_feeds

	"
