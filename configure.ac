#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([winmbf], [2.03])
AM_INIT_AUTOMAKE([foreign no-define])
AC_CONFIG_SRCDIR([Source/version.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_SEARCH_LIBS([pow], [m])
PKG_CHECK_MODULES([SDL],       [sdl])
PKG_CHECK_MODULES([SDL_mixer], [SDL_mixer])
PKG_CHECK_MODULES([SDL_net],   [SDL_net])
AC_DEFINE([MY_SDL_VER], [1], [This is WinMBF])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h malloc.h stddef.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_C_RESTRICT
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

AC_CONFIG_FILES([Makefile Source/Makefile])
AC_OUTPUT
