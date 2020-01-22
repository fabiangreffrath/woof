#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Woof], [0.9.0])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([Source/version.c])
AC_CONFIG_HEADERS([config.h])

PACKAGE_COPYRIGHT="Copyright (C) 1993-2020"
PACKAGE_LICENSE="GNU General Public License, version 2"
WINDOWS_RC_VERSION=`echo $PACKAGE_VERSION | sed 's/-.*//; s/\./, /g; s/$/, 0/'`
AC_SUBST(PACKAGE_COPYRIGHT)
AC_SUBST(PACKAGE_LICENSE)
AC_SUBST(WINDOWS_RC_VERSION)

# Checks for programs.
AC_PROG_CC
AC_CHECK_PROG(HAVE_PYTHON, python, true, false)

AC_CANONICAL_HOST
case "$host" in
  *-*-mingw* | *-*-cygwin* | *-*-msvc* )
    AC_CHECK_TOOL(WINDRES, windres, )
    ;;
  *)
    WINDRES=
    ;;
esac

AM_CONDITIONAL(HAVE_WINDRES, test "$WINDRES" != "")
AM_CONDITIONAL(HAVE_PYTHON, $HAVE_PYTHON)

# Checks for libraries.
AC_SEARCH_LIBS([pow], [m])
AC_SEARCH_LIBS([dlopen], [dl dld], [AC_CHECK_FUNCS([dlopen])])
PKG_CHECK_MODULES([SDL],       [sdl2])
PKG_CHECK_MODULES([SDL_mixer], [SDL2_mixer])
PKG_CHECK_MODULES([SDL_net],   [SDL2_net])
AC_DEFINE([RANGECHECK], [1], [Parameter Validation Debugging])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h malloc.h stddef.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_C_RESTRICT
AC_TYPE_INTPTR_T
AC_TYPE_UINTPTR_T
AC_TYPE_SIZE_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([atexit memmove memset mkdir pow putenv strcasecmp strchr strdup strerror strncasecmp strrchr strstr strtol])

AC_ARG_ENABLE([dogs], AS_HELP_STRING([--disable-dogs], [Disable support for helper dogs]))
AS_IF([test "x$enable_dogs" != "xno"], [AC_DEFINE([DOGS], [1], [Support for helper dogs])])

AC_ARG_ENABLE([beta], AS_HELP_STRING([--disable-beta], [Disable support for beta emulation]))
AS_IF([test "x$enable_beta" != "xno"], [AC_DEFINE([BETA], [1], [Support for beta emulation])])

# Set compiler flags
WARNINGS="-Wall -Wdeclaration-after-statement -Wredundant-decls"

AC_ARG_ENABLE([werror], AS_HELP_STRING([--enable-werror], [Treat warnings as errors]))
AS_IF([test "x$enable_werror" = "xyes"], [WARNINGS="-Werror $WARNINGS"])

if test "$GCC" = "yes"
then
	CFLAGS="$CFLAGS -g -O2 $WARNINGS"
fi

AC_CONFIG_FILES([
Makefile
configs/Makefile
data/Makefile
docs/Makefile
examples/Makefile
Source/Makefile
Source/resource.rc
toolsrc/Makefile
pkg/config.make
])
AC_OUTPUT
