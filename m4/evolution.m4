#  Evoution RSS Reader Plugin
#  Copyright (C) 2007-2008 Lucian Langa <cooly@gnome.eu.org>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

AC_DEFUN([EVOLUTION_INIT],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_REQUIRE([AC_PROG_AWK])dnl

AC_PROG_AWK


AC_MSG_CHECKING(Evolution version)
dnl is this a reasonable thing to do ?
EVOLUTION_VERSION=`$PKG_CONFIG --modversion evolution-shell 2>/dev/null`
if test -n "$EVOLUTION_VERSION"; then
        EVOLUTION_BASE_VERSION=$EVOLUTION_VERSION
        EVOLUTION_BASE_VERSION_S=""
        EVOLUTION_EXEC_VERSION=`$PKG_CONFIG --variable=execversion evolution-shell 2>/dev/null`
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
                EVOLUTION_VERSION=`$PKG_CONFIG --modversion evolution-shell-2.$i 2>/dev/null`
                if test -n "$EVOLUTION_VERSION"; then
                        EVOLUTION_BASE_VERSION=2.$i
                        EVOLUTION_BASE_VERSION_S="-"$EVOLUTION_BASE_VERSION
                        dnl this might be required for devel version
                        EVOLUTION_EXEC_VERSION=`$PKG_CONFIG --variable=execversion evolution-shell-2.$i 2>/dev/null`
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
AC_SUBST(EVOLUTION_EXEC_VERSION)

evolution_version_int="$(echo "$EVOLUTION_VERSION" | $AWK -F . '{print [$]1 * 10000 + [$]2 * 100 + [$]3}')"
if test "$evolution_version_int" -ge "21100"; then
        AC_DEFINE_UNQUOTED(EVOLUTION_2_12,1, [evolution mail 2.12 present])
        AC_SUBST(EVOLUTION_2_12)
fi
AC_SUBST(evolution_version_int)

MINOR_VERSION="$(echo $EVOLUTION_VERSION|cut -d. -f2|$AWK -F . '{print 1000 * [$]1}')"
AC_SUBST(MINOR_VERSION)

dnl Evolution plugin install directory
AC_ARG_WITH(plugin-install-dir, [  --with-plugin-install-dir=PATH path to evolution plugin directory])
if test "x$with_plugin_install_dir" = "x"; then
        PLUGIN_INSTALL_DIR=`$PKG_CONFIG --variable=plugindir evolution-plugin$EVOLUTION_BASE_VERSION_S`
        if test "x$PLUGIN_INSTALL_DIR" = "x"; then
                AC_MSG_ERROR(Unable to find plugin directory)
                break;
        fi
fi
AC_SUBST(PLUGIN_INSTALL_DIR)

dnl Evolution images directory
AC_ARG_WITH(icon-dir, [  --with-icon-dir=PATH path to evolution icon directory])
if test "x$with_icon_dir" = "x" ; then
   ICON_DIR=`$PKG_CONFIG --variable=imagesdir evolution-shell$EVOLUTION_BASE_VERSION_S`
      if test "x$ICON_DIR" = "x"; then
            AC_MSG_ERROR(Unable to find image directory)
       fi
fi
AC_SUBST(ICON_DIR)

dnl Evolution e-error install directory
ERROR_DIR=`$PKG_CONFIG --variable=errordir evolution-plugin$EVOLUTION_BASE_VERSION_S`
if test "x$ERROR_DIR" = "x"; then
   AC_MSG_ERROR(Unable to find error file directory)
fi
AC_SUBST(ERROR_DIR)

dnl test required for bonobo server installation
dnl dnl user might specify wrong prefix or not specify at all
AC_ARG_WITH(bonobo-servers-dir, [  --with-bonobo-servers-dir=PATH path to bonobo servers directory])
if test "x$with_bonobo_servers_dir" = "x" ; then
    BONOBO_LIBDIR=`$PKG_CONFIG --variable=libdir evolution-plugin$EVOLUTION_BASE_VERSION_S`
    if test "x$BONOBO_LIBDIR" = "x"; then
       AC_MSG_ERROR(Unable to find bonobo servers file directory)
    fi
fi
AC_SUBST(BONBONO_LIBDIR)

])
