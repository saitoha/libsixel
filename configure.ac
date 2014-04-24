#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
LT_PREREQ([2.4])
AC_INIT([sixel], [0.9.0], [user@zuse.jp])

AC_DEFUN([LS_UPDATE_TIMESTAMP], [
          touch aclocal.m4 Makefile.in */Makefile.in configure config.h.in
          ])
LS_UPDATE_TIMESTAMP

AM_INIT_AUTOMAKE([foreign])
LT_INIT
AC_CONFIG_SRCDIR([src/fromsixel.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
#AC_ENABLE_SHARED
#AC_DISABLE_STATIC

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Options
AC_ARG_ENABLE([img2sixel],
              AS_HELP_STRING([--disable-img2sixel], [do not build img2sixel]),
              [],
              [enable_img2sixel=yes])
AM_CONDITIONAL(WANT_IMG2SIXEL, test x$enable_img2sixel == xyes)

AC_ARG_ENABLE([sixel2png],
              AS_HELP_STRING([--disable-sixel2png], [do not build sixel2png]),
              [],
              [enable_sixel2img=yes])
AM_CONDITIONAL(WANT_SIXEL2IMG, test x$enable_sixel2img == xyes)

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([memory.h stdlib.h string.h unistd.h getopt.h])

# Checks for typedefs, structures, and compiler characteristics.
#AC_CHECK_HEADER_STDBOOL
#AC_TYPE_SIZE_T
#AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_CHECK_FUNC([memset], [],
              [AC_MSG_ERROR(memset is not available.)])

AC_CHECK_FUNC([getopt_long], [],
              [AC_MSG_ERROR(getopt_long is not available.)])

AC_CHECK_FUNCS([memcpy memmove strtol])

# for HDR
# AC_CHECK_FUNCS([strtol pow])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 include/Makefile
                 converters/Makefile])

AC_OUTPUT

