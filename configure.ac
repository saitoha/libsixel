#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
LT_PREREQ([2.4])
AC_INIT([sixel], [0.19.2], [user@zuse.jp])
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
AC_CHECK_PROG(have_pkg_config, pkg-config, yes, no)

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

AC_ARG_WITH([gdk-pixbuf2],
            [AS_HELP_STRING([--with-gdk-pixbuf2],
                            [whether to build with gdk-pixbuf2 (default: no)])],
            [],
            [with_gdk_pixbuf2=no])

AC_ARG_WITH([gd],
            [AS_HELP_STRING([--with-gd],
                            [whether to build with gd (default: no)])],
            [],
            [with_gd=no])

AC_ARG_WITH([libcurl],
            [AS_HELP_STRING([--with-libcurl],
                            [whether to build with libcurl (default: no)])],
            [],
            [with_libcurl=no])

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

loaders="stb_image"
curl="no"

if test x$with_gdk_pixbuf2 != xno; then
    if test x$have_pkg_config != xno; then
        PKG_CHECK_MODULES(GDK_PIXBUF,
                          [gdk-pixbuf-2.0],
                          [have_gdk_pixbuf2=yes],
                          [have_gdk_pixbuf2=no])
        if test x$have_gdk_pixbuf2 = xyes; then
            AC_DEFINE(HAVE_GDK_PIXBUF2, 1, [whether gdk-pixbuf2 is available])
            loaders+=" gdk-pixbuf2"
        else
            AC_MSG_ERROR(pkg-config is not available.)
        fi
    else
        AC_MSG_ERROR([pkg-config is missing])
    fi
fi
AC_SUBST(GDK_PIXBUF_CFLAGS)
AC_SUBST(GDK_PIXBUF_LIBS)


if test x$with_gd != xno; then
    AC_CHECK_HEADER([gd.h],
                    [AC_CHECK_LIB([gd],
                                  [gdImageCreateFromGifPtr],
                                  [have_gd="yes" GD_LIBS=-lgd],
                                  [have_gd="no"])])
    if test x$have_pkg_config != xno; then
        if test x$have_gd != xyes; then
            PKG_CHECK_MODULES(GD, [gdlib], [have_gd=yes], [have_gd=no])
        fi
    fi
    if test x$have_gd = xyes; then
        AC_DEFINE(HAVE_GD, 1, [whether gd is available])
        loaders+=" gd"
    else
        AC_MSG_ERROR(gd is not available.)
    fi
fi
AC_SUBST(GD_CFLAGS)
AC_SUBST(GD_LIBS)


if test x$with_libcurl != xno; then
    AC_CHECK_HEADER([curl/curl.h],
                    [AC_CHECK_LIB([curl],
                                  [curl_easy_cleanup],
                                  [have_curl=yes LIBCURL_LIBS=-lcurl],
                                  [have_curl=no])])
    if test x$have_curl != xyes; then
        if test x$have_pkg_config != xno; then
            PKG_CHECK_MODULES(LIBCURL,
                              [libcurl],
                              [have_curl=yes],
                              [have_curl=no])
        fi
    fi
    if test x$have_curl = xyes; then
        AC_DEFINE(HAVE_LIBCURL, 1, [whether libcurl is available])
    else
        AC_MSG_ERROR([unable to find libcurl])
    fi
fi
AC_SUBST(LIBCURL_CFLAGS)
AC_SUBST(LIBCURL_LIBS)


# for HDR
# AC_CHECK_FUNCS([strtol pow])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 include/Makefile
                 converters/Makefile])

AC_OUTPUT

echo ""
echo ""
echo "libsixel was configured as follows"
echo ""
echo "       Loader component    : $loaders"
echo "       libcurl integration : $have_curl"
echo ""
echo ""

