#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
LT_PREREQ([2.4])
AC_INIT([sixel], [0.17.2], [user@zuse.jp])
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
                              [whether to build img2sixel (default: yes)])],
              [],
              [enable_img2sixel=yes])
AM_CONDITIONAL(WANT_IMG2SIXEL, test x$enable_img2sixel != xno)

AC_ARG_ENABLE([sixel2png],
              [AS_HELP_STRING([--disable-sixel2png],
                              [whether to build sixel2png (default: yes)])],
              [],
              [enable_sixel2png=yes])
AM_CONDITIONAL(WANT_SIXEL2PNG, test x$enable_sixel2png != xno)

AC_ARG_ENABLE([gdk-pixbuf2],
              [AS_HELP_STRING([--enable-gdk-pixbuf2],
                              [whether to build with gdk-pixbuf2 (default: no)])],
              [],
              [enable_gdk_pixbuf2=no])

AC_ARG_ENABLE([libcurl],
              [AS_HELP_STRING([--enable-libcurl],
                              [whether to build with libcurl (default: no)])],
              [],
              [enable_libcurl=no])

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

PKG_CHECK_MODULES(GDK_PIXBUF, [gdk-pixbuf-2.0], [have_gdk_pixbuf2=yes], [have_gdk_pixbuf2=no])
AC_SUBST(GDK_PIXBUF_CFLAGS)
AC_SUBST(GDK_PIXBUF_LIBS)
AM_CONDITIONAL(HAVE_GDK_PIXBUF, [test x$enable_gdk_pixbuf2 != xno && test x$have_gdk_pixbuf2 = xyes])

if test x$enable_gdk_pixbuf2 != xno; then
    if test x$have_gdk_pixbuf2 = xyes; then
        AC_DEFINE(HAVE_GDK_PIXBUF2, 1, [whether gdk-pixbuf2 is available])
    else
        AC_MSG_ERROR(gdk-pixbuf2 is not available.)
    fi
fi

PKG_CHECK_MODULES(LIBCURL, [libcurl], [have_libcurl=yes], [have_libcurl=no])
AC_SUBST(LIBCURL_CFLAGS)
AC_SUBST(LIBCURL_LIBS)
AM_CONDITIONAL(HAVE_LIBCURL, [test x$enable_libcurl != xno && test x$have_libcurl = xyes])

if test x$enable_libcurl != xno; then
    if test x$have_libcurl = xyes; then
        AC_DEFINE(HAVE_LIBCURL, 1, [whether libcurl is available])
    else
        AC_MSG_ERROR(libcurl is not available.)
    fi
fi

# for HDR
# AC_CHECK_FUNCS([strtol pow])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 include/Makefile
                 converters/Makefile])

AC_OUTPUT

