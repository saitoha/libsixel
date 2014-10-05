#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
LT_PREREQ([2.4])
AC_INIT([sixel], [1.1.4], [user@zuse.jp])
LS_LT_CURRENT=1
LS_LT_REVISION=0
LS_LT_AGE=1
AC_SUBST([LS_LTVERSION], [$LS_LT_CURRENT:$LS_LT_REVISION:$LS_LT_AGE])
AC_SUBST([PACKAGE_DESCRIPTION],
         ["A lightweight, fast implementation of DEC SIXEL graphics codec"])

AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([foreign])
LT_INIT
AC_CONFIG_SRCDIR([src/fromsixel.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
#AC_ENABLE_SHARED
#AC_DISABLE_STATIC

AX_GCC_VAR_ATTRIBUTE([deprecated])
if test x$ax_cv_have_func_attribute_deprecated != x; then
    AC_SUBST(attr_var_deprecated, [__attribute__\(\(deprecated\)\)])
fi

AX_GCC_FUNC_ATTRIBUTE([deprecated])
if test x$ax_cv_have_func_attribute_deprecated != x; then
    AC_SUBST(attr_func_deprecated, [__attribute__\(\(deprecated\)\)])
fi

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

#AC_ARG_WITH([wic],
#            [AS_HELP_STRING([--with-wic],
#                            [whether to build with windows WIC integration, windows only (default: no)])],
#            [],
#            [with_wic=no])

AC_ARG_WITH(pkgconfigdir,
            AC_HELP_STRING([--with-pkgconfigdir],
                           [Use the specified pkgconfig dir (default is libdir/pkgconfig)]),
                           [pkgconfigdir=${withval}],
                           [pkgconfigdir=${libdir}/pkgconfig])
AC_MSG_NOTICE([pkgconfig directory is ${pkgconfigdir}])
AC_SUBST(pkgconfigdir)

AC_ARG_WITH(bashcompletiondir,
            AC_HELP_STRING([--with-bashcompletiondir],
                           [Use the specified bashcompletion.d]),
                           [bashcompletiondir=${withval}],
                           [bashcompletiondir=${datadir}/bash-completion/completions])
AC_MSG_NOTICE([bash-completion directory is ${bashcompletiondir}])
AC_SUBST(bashcompletiondir)

AC_ARG_WITH(zshcompletiondir,
            AC_HELP_STRING([--with-zshcompletiondir],
                           [Use the specified zshcompletion.d]),
                           [zshcompletiondir=${withval}],
                           [zshcompletiondir=${datadir}/zsh/site-functions])
AC_MSG_NOTICE([zsh-completion directory is ${zshcompletiondir}])
AC_SUBST(zshcompletiondir)

AC_ARG_ENABLE([gcov],
              [AS_HELP_STRING([--enable-gcov],
                              [Use gcov])],
              [],
              [enable_gcov=no])
AM_CONDITIONAL([COND_GCOV], [test x$enable_gcov != xno])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG
AC_CHECK_PROG(have_pkg_config, pkg-config, yes, no)

# For wic integration
#AC_PROG_CXX

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([memory.h \
                  stdlib.h \
                  string.h \
                  unistd.h \
                  sys/unistd.h \
                  getopt.h \
                  sys/types.h \
                  sys/stat.h \
                  fcntl.h \
                  errno.h \
                  sys/time.h \
                  time.h \
                  signal.h \
                  sys/signal.h])

# Checks for typedefs, structures, and compiler characteristics.
#AC_CHECK_HEADER_STDBOOL
#AC_TYPE_SIZE_T
#AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_CHECK_FUNC([memset], [],
              [AC_MSG_ERROR(memset is not available.)])

AC_CHECK_FUNC([getopt_long],
              [AC_DEFINE(HAVE_GETOPT_LONG, 1, [whether getopt_long is avilable])],
              [AC_CHECK_FUNC([getopt],
                             [AC_DEFINE(HAVE_GETOPT, 1, [whether getopt is avilable])],
                             [AC_MSG_ERROR(getopt or getopt_long is not available.)])])

AC_CHECK_FUNCS([memcpy memmove strtol setmode _setmode signal usleep calloc clock])

# for HDR
AC_CHECK_FUNCS([strtol pow])

AC_CHECK_DECLS([SIGINT, SIGTERM, SIGHUP],,,
               [
                   #ifdef HAVE_SIGNAL_H
                   # include <signal.h>
                   #elif HAVE_SYS_SIGNAL_H
                   # include <sys/signal.h>
                   #endif
               ])

loaders="stb_image"
have_curl="no"
#have_wic="no"

#if test x$with_wic != xno; then
#    AC_CHECK_HEADERS([wincodec.h], [],
#                     [AC_MSG_ERROR([WIC integration: wincodec.h is not avilable.])])
#    AC_CHECK_LIB([ole32], [main], [],
#                 [AC_MSG_ERROR([WIC integration: library ole32 is not avilable.])])
#    AC_CHECK_LIB([uuid], [main], [],
#                 [AC_MSG_ERROR([WIC integration: library uuid is not avilable.])])
#    AC_CHECK_LIB([windowscodecs], [main], [],
#                 [AC_MSG_ERROR([WIC integration: library wincodecs is not avilable.])])
#    have_wic="yes"
#fi

if test x$with_gdk_pixbuf2 != xno; then
    if test x$have_pkg_config != xno; then
        PKG_CHECK_MODULES([GDK_PIXBUF],
                          [gdk-pixbuf-2.0],
                          [have_gdk_pixbuf2=yes],
                          [have_gdk_pixbuf2=no])
        if test x$have_gdk_pixbuf2 != xno; then
            AC_MSG_NOTICE([gdk-pixbuf: $have_gdk_pixbuf2])
            AC_DEFINE(HAVE_GDK_PIXBUF2, 1, [whether gdk-pixbuf2 is available])
            loaders="${loaders} gdk-pixbuf2"
        else
            AC_MSG_ERROR([pkg-config is not available.])
        fi
    else
        if test x${GDK_PIXBUF_CFLAGS} = x || test x${GDK_PIXBUF_LIBS} = x; then
            AC_MSG_ERROR([please set GDK_PIXBUF_CFLAGS and GDK_PIXBUF_LIBS, or install pkg-config.])
        fi
    fi
    AC_MSG_NOTICE([gdk-pixbuf-2.0 cflags is ${GDK_PIXBUF_CFLAGS}])
    AC_MSG_NOTICE([gdk-pixbuf-2.0 libs is ${GDK_PIXBUF_LIBS}])
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
        AC_CHECK_LIB([gd],
                     [gdImageCreateFromGifPtr],
                     [AC_CHECK_DECLS([gdImageCreateFromGifPtr],   [], [], [ #include <gd.h> ])])
        AC_CHECK_LIB([gd],
                     [gdImageCreateFromBmpPtr],
                     [AC_CHECK_DECLS([gdImageCreateFromPngPtr],   [], [], [ #include <gd.h> ])])
        AC_CHECK_LIB([gd],
                     [gdImageCreateFromBmpPtr],
                     [AC_CHECK_DECLS([gdImageCreateFromBmpPtr],   [], [], [ #include <gd.h> ])])
        AC_CHECK_LIB([gd],
                     [gdImageCreateFromJpegPtrEx],
                     [AC_CHECK_DECLS([gdImageCreateFromJpegPtrEx],[], [], [ #include <gd.h> ])])
        AC_CHECK_LIB([gd],
                     [gdImageCreateFromJpegPtr],
                     [AC_CHECK_DECLS([gdImageCreateFromJpegPtr],  [], [], [ #include <gd.h> ])])
        AC_CHECK_LIB([gd],
                     [gdImageCreateFromTgaPtr],
                     [AC_CHECK_DECLS([gdImageCreateFromTgaPtr],   [], [], [ #include <gd.h> ])])
        AC_CHECK_LIB([gd],
                     [gdImageCreateFromWBMPPtr],
                     [AC_CHECK_DECLS([gdImageCreateFromWBMPPtr],  [], [], [ #include <gd.h> ])])
        AC_CHECK_LIB([gd],
                     [gdImageCreateFromTiffPtr],
                     [AC_CHECK_DECLS([gdImageCreateFromTiffPtr], [], [], [ #include <gd.h> ])])
        AC_CHECK_LIB([gd],
                     [gdImageCreateFromGd2Ptr],
                     [AC_CHECK_DECLS([gdImageCreateFromGd2Ptr],   [], [], [ #include <gd.h> ])])
        AC_CHECK_LIB([gd],
                     [gdImagePaletteToTrueColor],
                     [AC_CHECK_DECLS([gdImagePaletteToTrueColor], [], [], [ #include <gd.h> ])])
        AC_DEFINE(HAVE_GD, 1, [whether gd is available])
        loaders="${loaders} gd"
    else
        AC_MSG_ERROR(gd is not available.)
    fi

    case $host in
        *-*-cygwin)
            AC_CHECK_LIB(iconv, [libiconv], [],
                         [AC_CHECK_LIB(iconv, [iconv], [],
                                       [AC_MSG_ERROR([please install libiconv])])
                         ])
            ;;
    esac

    AC_MSG_NOTICE([gd cflags is ${GD_CFLAGS}])
    AC_MSG_NOTICE([gd libs is ${GD_LIBS}])
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
    AC_MSG_NOTICE([libcurl cflags is ${LIBCURL_CFLAGS}])
    AC_MSG_NOTICE([libcurl libs is ${LIBCURL_LIBS}])
fi
AC_SUBST(LIBCURL_CFLAGS)
AC_SUBST(LIBCURL_LIBS)

AC_DEFUN([LS_UPDATE_TIMESTAMP], [
          touch aclocal.m4 Makefile.in */Makefile.in configure config.h.in
          ])
LS_UPDATE_TIMESTAMP

AC_CONFIG_FILES([Makefile
                 libsixel.pc
                 package.json.in
                 include/sixel.h
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
echo "       pkg-config dir      : $pkgconfigdir"
echo "       Bash completion dir : $bashcompletiondir"
echo "       Zsh completion dir  : $zshcompletiondir"
#echo "       WIC integration     : $have_wic"
echo "       gcov integration    : $enable_gcov"
echo ""
echo ""
