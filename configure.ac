#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
LT_PREREQ([2.4])
AC_INIT([sixel], [0.15.0], [user@zuse.jp])
LS_LT_CURRENT=0
LS_LT_REVISION=0
LS_LT_AGE=0
AC_SUBST([LS_LTVERSION], [$LS_LT_CURRENT:$LS_LT_REVISION:$LS_LT_AGE])


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
              [AS_HELP_STRING([--disable-img2sixel],
                              [whether to build img2sixel (default: enable)])],
              [],
              [enable_img2sixel=yes])
AM_CONDITIONAL(WANT_IMG2SIXEL, test x$enable_img2sixel != xno)

AC_ARG_ENABLE([sixel2png],
              [AS_HELP_STRING([--disable-sixel2png],
                              [whether to build sixel2png (default: enable)])],
              [],
              [enable_sixel2png=yes])
AM_CONDITIONAL(WANT_SIXEL2PNG, test x$enable_sixel2png != xno)

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([memory.h \
		  stdlib.h \
		  string.h \
		  unistd.h \
		  getopt.h \
		  sys/types.h \
		  sys/stat.h \
		  fcntl.h \
		  errno.h \
		  xmmintrin.h])

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

AC_CHECK_FUNCS([memcpy memmove strtol setmode _setmode])

AC_CHECK_FUNC(memalign,
	      [AC_DEFINE(HAVE_MEMALIGN, 1, [whether memalign() is available])])
AC_CHECK_FUNC(posix_memalign,
	      [AC_DEFINE(HAVE_POSIX_MEMALIGN, 1, [whether posix_memalign() is available])])
AC_CHECK_FUNC(aligned_alloc,
	      [AC_DEFINE(HAVE_ALIGNED_ALLOC, 1, [whether aligned_alloc() is available])])
AC_CHECK_FUNC(_aligned_malloc,
	      [AC_DEFINE(HAVE__ALIGNED_MALLOC, 1, [whether _aligned_malloc() is available])])

# for HDR
# AC_CHECK_FUNCS([strtol pow])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 include/Makefile
                 converters/Makefile])

AC_OUTPUT

