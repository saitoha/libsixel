#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
LT_PREREQ([2.4])
AC_INIT([libsixel], [1.8.7], [saitoha@me.com])
LS_LT_CURRENT=1
LS_LT_REVISION=6
LS_LT_AGE=0
AC_SUBST([LS_LTVERSION], [$LS_LT_CURRENT:$LS_LT_REVISION:$LS_LT_AGE])
AC_SUBST([PACKAGE_DESCRIPTION],
         ["A lightweight, fast implementation of DEC SIXEL graphics codec"])

AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])
LT_INIT
AC_CONFIG_SRCDIR([src/fromsixel.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Detect Microsoft Visual C compiler
case "$CC" in
  *cl*|*clang-cl*)
    using_cl=yes
    CFLAGS=`echo "$CFLAGS" | sed s/-g//g`
    CFLAGS="$CFLAGS -O2"
    LDFLAGS="$LDFLAGS -DEBUG"
    PKG_CONFIG=
    ;;
  *)
    using_cl=no
    ;;
esac
AM_CONDITIONAL([USING_CL], [test "x$using_cl" = "xyes"])

AC_CACHE_CHECK(
  [whether the compiler can silence -Wtypedef-redefinition via pragmas],
  [ac_cv_have_diag_typedef_redef],
  [
   ac_save_CFLAGS="$CFLAGS"
   CFLAGS="$CFLAGS -Werror"
   AC_COMPILE_IFELSE(
     [AC_LANG_SOURCE([[
       #if defined(__clang__) || defined(__GNUC__)
       #pragma clang diagnostic push
       #pragma clang diagnostic ignored "-Wtypedef-redefinition"
       typedef int foo;
       typedef int foo;
       #pragma clang diagnostic pop
       #endif
     ]])],
     [ac_cv_have_diag_typedef_redef=yes],
     [ac_cv_have_diag_typedef_redef=no])
   CFLAGS="$ac_save_CFLAGS"
  ])

AS_IF([test "x$ac_cv_have_diag_typedef_redef" = "xyes"], [
  AC_DEFINE([HAVE_DIAGNOSTIC_TYPEDEF_REDEFINITION],
            [1],
            [Define to 1 if the compiler can ignore -Wtypedef-redefinition via pragmas])
])

AC_CACHE_CHECK(
  [whether the compiler can silence -Wdeprecated-declarations],
  [ac_cv_have_diag_deprecated_declarations],
  [
   ac_save_CFLAGS="$CFLAGS"
   CFLAGS="$CFLAGS -Werror"
   AC_COMPILE_IFELSE(
     [AC_LANG_SOURCE([[
       #if defined(__clang__) || defined(__GNUC__)
       #pragma clang diagnostic push
       #pragma clang diagnostic ignored "-Wdeprecated-declarations"
       typedef int foo;
       typedef int foo;
       #pragma clang diagnostic pop
       #endif
     ]])],
     [ac_cv_have_diag_deprecated_declarations=yes],
     [ac_cv_have_diag_deprecated_declarations=no])
   CFLAGS="$ac_save_CFLAGS"
  ])

AS_IF([test "x$ac_cv_have_diag_deprecated_declarations" = "xyes"], [
  AC_DEFINE([HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS],
            [1],
            [Define to 1 if the compiler can ignore -Wdeprecated-declarations via pragmas])
])

# Options
AC_ARG_ENABLE([img2sixel],
              [AS_HELP_STRING([--disable-img2sixel],
                              [disable building the img2sixel utility (default: no)])],
              [],
              [enable_img2sixel=yes])
AM_CONDITIONAL(WANT_IMG2SIXEL, test x$enable_img2sixel != xno)

AC_ARG_ENABLE([sixel2png],
              [AS_HELP_STRING([--disable-sixel2png],
                              [disable building the sixel2png utility (default: no)])],
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

AC_ARG_WITH([coregraphics],
            [AS_HELP_STRING([--with-coregraphics],
                            [whether to build with CoreGraphics (default: auto)])],
            [],
            [with_coregraphics=auto])

AC_ARG_WITH([libcurl],
            [AS_HELP_STRING([--with-libcurl],
                            [whether to build with libcurl (default: auto)])],
            [],
            [with_libcurl=auto])

AC_ARG_WITH([jpeg],
            [AS_HELP_STRING([--with-jpeg],
                            [whether to build with libjpeg (default: auto)])],
            [],
            [with_jpeg=auto])

AC_ARG_WITH([png],
            [AS_HELP_STRING([--with-png],
                            [whether to build with libpng (default: auto)])],
            [],
            [with_png=auto])

AC_ARG_WITH([winpthread],
            [AS_HELP_STRING([--with-winpthread],
                            [whether to build with libwinpthread, windows only (default: no)])],
            [],
            [with_winpthread=no])

AC_ARG_WITH([wic],
            [AS_HELP_STRING([--with-wic],
                            [whether to build with Windows Imaging Component (WIC), windows only (default: auto)])],
            [],
            [with_wic=auto])

AC_ARG_WITH([winhttp],
            [AS_HELP_STRING([--with-winhttp],
                            [whether to build with WinHTTP, windows only (default: auto)])],
            [],
            [with_winhttp=auto])

AC_ARG_WITH(pkgconfigdir,
            AS_HELP_STRING([--with-pkgconfigdir],
                           [Use the specified pkgconfig dir (default is libdir/pkgconfig)]),
                           [pkgconfigdir=${withval}],
                           [pkgconfigdir=${libdir}/pkgconfig])
AC_MSG_NOTICE([pkgconfig directory is ${pkgconfigdir}])
AC_SUBST(pkgconfigdir)

AC_ARG_WITH(bashcompletiondir,
            AS_HELP_STRING([--with-bashcompletiondir],
                           [Use the specified bashcompletion.d]),
                           [bashcompletiondir=${withval}],
                           [bashcompletiondir=${datadir}/bash-completion/completions])
AC_MSG_NOTICE([bash-completion directory is ${bashcompletiondir}])
AC_SUBST(bashcompletiondir)

AC_ARG_WITH(zshcompletiondir,
            AS_HELP_STRING([--with-zshcompletiondir],
                           [Use the specified zshcompletion.d]),
                           [zshcompletiondir=${withval}],
                           [zshcompletiondir=${datadir}/zsh/site-functions])
AC_MSG_NOTICE([zsh-completion directory is ${zshcompletiondir}])
AC_SUBST(zshcompletiondir)

AC_ARG_ENABLE([simd],
              [AS_HELP_STRING([--enable-simd],
                              [enable SS2 and NEON (default: auto)])],
              [],
              [])

AC_ARG_ENABLE([wiccodec],
              [AS_HELP_STRING([--enable-wiccodec],
                              [build WIC codec DLL (default: auto)])],
              [],
              [])

AC_ARG_ENABLE([python],
              [AS_HELP_STRING([--enable-python],
                              [Python interface (default: yes)])],
              [],
              [])

AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug],
                              [Use debug macro and specific CFLAGS])],
              [AC_DEFINE(HAVE_DEBUG, [1], [enable debugging support])],
              [enable_debug=no])
AM_CONDITIONAL([COND_DEBUG], [test x$enable_debug != xno])

AC_ARG_ENABLE([gcov],
              [AS_HELP_STRING([--enable-gcov],
                              [Use gcov])],
              [],
              [enable_gcov=no])
AM_CONDITIONAL([COND_GCOV], [test x$enable_gcov != xno])

AC_ARG_ENABLE([tests],
              [AS_HELP_STRING([--enable-tests],
                              [Build tests])],
              [AC_DEFINE(HAVE_TESTS, [1], [enable tests])],
              [enable_tests=no])
AM_CONDITIONAL([COND_TESTS], [test x$enable_tests != xno])

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AS_IF([test x$cross_compile != xyes],
      [PKG_PROG_PKG_CONFIG([],
                           [AC_MSG_WARN([pkg-config not found, continuing without it])
                            PKG_CONFIG=false])],
      [])

AC_DEFUN([LS_CHECK_CFLAG],
         [AC_CACHE_CHECK([if $CC supports $1],
                         [ac_cv_cc_flag_[]AS_TR_SH([$4])],
                         [saved_cflags="$CFLAGS"
                          CFLAGS="$1 -Werror"
                          AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],
                                            [ac_cv_cc_flag_$4=yes],
                                            [ac_cv_cc_flag_$4=no])
                          CFLAGS="$saved_cflags"])
          AS_IF([eval test \"x\$ac_cv_cc_flag_[]AS_TR_SH([$4])\" = xyes],
                [m4_default([$2], :)],
                [m4_default([$3], :)])
         ])

if test "x$using_cl" = "xyes"; then
  CPPFLAGS="$CPPFLAGS -D_CRT_SECURE_NO_WARNINGS"
  CFLAGS="$CFLAGS -nologo"
  AC_DEFINE([HAVE_MSVC], [1], [Define if building with MSVC])
else
  AX_GCC_VAR_ATTRIBUTE([deprecated])
  if test x$ax_cv_have_var_attribute_deprecated != x; then
      AC_SUBST(attr_var_deprecated, [__attribute__\(\(deprecated\)\)])
  fi
  AX_GCC_FUNC_ATTRIBUTE([deprecated])
  if test x$ax_cv_have_func_attribute_deprecated != x; then
      AC_SUBST(attr_func_deprecated, [__attribute__\(\(deprecated\)\)])
  fi
  AX_GCC_BUILTIN([__builtin_unreachable])
  CPPFLAGS="$CPPFLAGS -D_POSIX_SOURCE -D_POSIX_C_SOURCE=199309L"
  LS_CHECK_CFLAG([-std=c99],
                 [CFLAGS="$CFLAGS -std=c99"],
                 [],
                 [std_c99])
  LS_CHECK_CFLAG([-Wall],
                 [AM_CFLAGS="$AM_CFLAGS -Wall"],
                 [],
                 [wall])
  LS_CHECK_CFLAG([-Wextra],
                 [AM_CFLAGS="$AM_CFLAGS -Wextra"],
                 [],
                 [wextra])
  LS_CHECK_CFLAG([-Wformat=2],
                 [AM_CFLAGS="$AM_CFLAGS -Wformat=2"],
                 [],
                 [wformat_2])
  LS_CHECK_CFLAG([-Wsign-conversion],
                 [AC_DEFINE(HAVE_DIAGNOSTIC_SIGN_CONVERSION,
                            [1],
                            [define 1 if GCC supports -Wsign-conversion])],
                 [],
                 [wsign_conversion])
  LS_CHECK_CFLAG([-Wuninitialized],
                 [AC_DEFINE(HAVE_DIAGNOSTIC_UNINITIALIZED,
                            [1],
                            [define 1 if GCC supports -Wuninitialized])],
                 [],
                 [wuninitialized])
  LS_CHECK_CFLAG([-Wstrict-overflow],
                 [AC_DEFINE(HAVE_DIAGNOSTIC_STRICT_OVERFLOW,
                            [1],
                            [define 1 if GCC supports -Wstrict-overeflow=1])],
                 [],
                 [wstrict_overflow])
  LS_CHECK_CFLAG([-Wshadow],
                 [AC_DEFINE(HAVE_DIAGNOSTIC_SHADOW,
                            [1],
                            [define 1 if GCC supports -Wshadow])],
                 [],
                 [wshadow])
  LS_CHECK_CFLAG([-Wdouble-promotion],
                 [AC_DEFINE(HAVE_DIAGNOSTIC_DOUBLE_PROMOTION,
                            [1],
                            [define 1 if GCC supports -Wdouble-promotion])],
                 [],
                 [wdouble_promotions])
  LS_CHECK_CFLAG([-Wswitch-default],
                 [AC_DEFINE(HAVE_DIAGNOSTIC_SWITCH_DEFAULT,
                            [1],
                            [define 1 if GCC supports -Wswitch-default])],
                 [],
                 [wswitch_default])
  LS_CHECK_CFLAG([-Wunused-function],
                 [AC_DEFINE(HAVE_DIAGNOSTIC_UNUSED_FUNCTION,
                            [1],
                            [define 1 if GCC supports -Wunused-function])],
                 [],
                 [wunused_function])
  LS_CHECK_CFLAG([-Wclobbered],
                 [AC_DEFINE(HAVE_DIAGNOSTIC_CLOBBERED,
                            [1],
                            [define 1 if GCC supports -Wclobbered])],
                 [],
                 [wclobbered])
  LS_CHECK_CFLAG([-Wunused-but-set-variable],
                 [AC_DEFINE(HAVE_DIAGNOSTIC_UNUSED_BUT_SET_VARIABLE,
                            [1],
                            [define 1 if GCC supports -Wunused-but-set-variable])],
                 [],
                 [wunused_but_set_variable])
  LS_CHECK_CFLAG([-Bsymbolic],
                 [AM_CFLAGS="$AM_CFLAGS -Bsymbolic"
                  AC_DEFINE(HAVE_BSYMBOLIC,
                            [1],
                            [define 1 if GCC supports -Bsymbolic])],
                 [],
                 [bsymbolic])
  LS_CHECK_CFLAG([-fno-emulated-tls],
                 [AM_CFLAGS="$AM_CFLAGS -Bfno-emulated-tls"
                  AC_DEFINE(HAVE_FNO_EMULATED_TLS,
                            [1],
                            [define 1 if GCC supports -fno-emulated-tls])],
                 [],
                 [fno_emulated_tls])
  AS_IF([test x$enable_debug = xyes],
        [LS_CHECK_CFLAG([-Werror], [AM_CFLAGS="$AM_CFLAGS -Werror"], [])])
fi
AC_SUBST([AM_CFLAGS])

# For test
AC_CHECK_PROGS(MD5SUM, [md5sum md5])
AC_SUBST(MD5SUM)

# Check math library
AC_CHECK_LIB([m], [floor])

# Checks for header files.
AC_CHECK_HEADERS([assert.h \
                  math.h \
                  ctype.h \
                  memory.h \
                  unistd.h \
                  stdint.h \
                  sys/unistd.h \
                  getopt.h \
                  sys/types.h \
                  sys/stat.h \
                  fcntl.h \
                  io.h \
                  errno.h \
                  limits.h \
                  sys/time.h \
                  time.h \
                  signal.h \
                  sys/select.h \
                  sys/signal.h \
                  termios.h \
                  sys/ioctl.h \
                  inttypes.h \
                  emmintrin.h \
                  arm_neon.h \
                 ])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_CHECK_FUNC([getopt_long],
              [AC_DEFINE(HAVE_GETOPT_LONG, 1, [whether getopt_long is avilable])
               have_getopt_long=1],
              [AC_CHECK_FUNC([getopt],
                             [AC_DEFINE(HAVE_GETOPT, 1, [whether getopt is avilable])],
                             [])])

if test "x$using_cl" = "xyes"; then
    AC_DEFINE([HAVE_MEMCPY], [1])
    AC_DEFINE([HAVE_MEMMOVE], [1])
    AC_DEFINE([HAVE_MEMSET], [1])
else
    AC_CHECK_FUNCS([memcpy \
                    memmove \
                    memset \
                   ])
fi

AC_CHECK_FUNCS([setmode \
                _setmode \
                signal \
                strdup \
                strtoul \
                calloc \
                clearerr \
                stat \
                setjmp \
                longjmp \
                strerror \
                isatty \
                strncmp \
                ldiv \
                floor \
                localeconv \
                pow \
                select \
                sqrt \
                strchr \
                strerror \
                strstr \
                strtol])

case "${host_os}" in
  mingw*)
    ;;
  *)
    AC_CHECK_FUNCS([clock nanosleep])
    ;;
esac

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
have_jpeg="no"
have_png="no"
have_winpthread="no"
have_wic="no"
have_coregraphics="no"
have_sse2=no
have_neon=no

REQUIRES_PRIVATE=""
LIBS_PRIVATE="$LIBS"

if test x$with_gdk_pixbuf2 != xno; then
    AC_MSG_NOTICE([checking for gdk-pixbuf2])
    if test x${PKG_CONFIG} != x; then
        PKG_CHECK_MODULES([GDK_PIXBUF],
                          [gdk-pixbuf-2.0],
                          [have_gdk_pixbuf2=yes],
                          [have_gdk_pixbuf2=no])
        if test x$have_gdk_pixbuf2 != xno; then
            AC_DEFINE(HAVE_GDK_PIXBUF2, 1, [whether gdk-pixbuf2 is available])
            loaders="${loaders} gdk-pixbuf2"
            REQUIRES_PRIVATE="$REQUIRES_PRIVATE gdk-pixbuf-2.0"
            LIBS_PRIVATE="$LIBS_PRIVATE $GDK_PIXBUF_LIBS"
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

if test x$with_coregraphics != xno; then
    saved_LIBS="$LIBS"
    LIBS="$LIBS -framework CoreGraphics -framework ImageIO"
    AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([[#include <CoreGraphics/CoreGraphics.h>
                           #include <ImageIO/ImageIO.h>]],
                         [[CGImageRef image = NULL;]])],
        [have_coregraphics=yes],
        [have_coregraphics=no])
    LIBS="$saved_LIBS"
    if test x$have_coregraphics = xyes; then
        AC_DEFINE(HAVE_COREGRAPHICS, 1, [whether CoreGraphics is available])
        COREGRAPHICS_LIBS="-framework CoreFoundation -framework CoreGraphics -framework ImageIO"
        COREGRAPHICS_CFLAGS=""
        loaders="${loaders} coregraphics"
        LIBS_PRIVATE="$LIBS_PRIVATE $COREGRAPHICS_LIBS"
        AC_MSG_NOTICE([coregraphics libs is ${COREGRAPHICS_LIBS}])
    elif test x$with_coregraphics = xyes; then
        AC_MSG_ERROR([unable to find CoreGraphics])
    fi
fi
AC_SUBST(COREGRAPHICS_CFLAGS)
AC_SUBST(COREGRAPHICS_LIBS)

if test x$with_gd != xno && test "x$using_cl" != "xyes"; then
    AC_MSG_NOTICE([checking for GD])
    CFLAGS_BACKUP=$CFLAGS
    LDFLAGS_BACKUP=$LDFLAGS
    if test x$with_gd != xyes -a x$with_gd != xauto; then
        if test ! -d "$with_gd"; then
            AC_MSG_ERROR(["${with_gd}" is not directory])
        fi
        ADDED_CFLAGS="-I${with_gd}/include"
        ADDED_LIBS="-L${with_gd}/lib -lgd"
    else
        if test x${prefix} != x -a x${prefix} != xNONE; then
            ADDED_CFLAGS="-I${prefix}/include"
            ADDED_LIBS="-L${prefix}/lib -lgd"
        else
            ADDED_CFLAGS="-I/usr/local/include"
            ADDED_LIBS="-L/usr/local/lib -lgd"
        fi
    fi
    CFLAGS="${CFLAGS} ${GD_CFLAGS}"
    LDFLAGS="${LDFLAGS} ${GD_LIBS}"
    AC_CHECK_HEADER([gd.h],
                    [AC_CHECK_LIB([gd],
                                  [gdImageCreateFromGifPtr],
                                  [have_gd="yes"
                                   GD_CFLAGS=$ADDED_CFLAGS
                                   GD_LIBS=$ADDED_LIBS],
                                  [have_gd="no"])])
    CFLAGS=$CFLAGS_BACKUP
    LDFLAGS=$LDFLAGS_BACKUP
    if test x${PKG_CONFIG} != x; then
        if test x$have_gd != xyes -a ! -d "$with_gd"; then
            PKG_CHECK_MODULES(GD, [gdlib], [have_gd=yes], [have_gd=no])
        fi
    fi
    if test x$have_gd = xyes; then
        CFLAGS_BACKUP=$CFLAGS
        LDFLAGS_BACKUP=$LDFLAGS
        CFLAGS="$CFLAGS $GD_CFLAGS"
        LDFLAGS="${LDFLAGS} ${GD_LIBS}"
        AC_CHECK_LIB([gd],
                     [gdImageCreateFromGifPtr],
                     [AC_CHECK_DECLS([gdImageCreateFromGifPtr],   [], [], [ #include <gd.h> ])])
        AC_CHECK_LIB([gd],
                     [gdImageCreateFromGifAnimPtr],
                     [AC_CHECK_DECLS([gdImageCreateFromGifAnimPtr], [], [], [ #include <gd.h> ])])
        AC_CHECK_LIB([gd],
                     [gdImageCreateFromPngPtr],
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
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        AC_DEFINE(HAVE_GD, 1, [whether gd is available])
        loaders="${loaders} gd"
        REQUIRES_PRIVATE="$REQUIRES_PRIVATE gdlib"
        LIBS_PRIVATE="$LIBS_PRIVATE $GD_LIBS"
    else
        GD_CFLAGS=
        GD_LIBS=
        AC_MSG_ERROR(gd is not available.)
    fi

    case $host in
        *-*-cygwin)
            AC_CHECK_LIB(iconv, [libiconv], [],
                         [AC_CHECK_LIB(iconv, [iconv], [],
                                       [AC_MSG_ERROR([please install libiconv])])
                         ])
            ;;
        *-*-interix)
            AM_CPPFLAGS="-D_ALL_SOURCES"
            AC_SUBST([AM_CPPFLAGS])
            ;;
    esac

    AC_MSG_NOTICE([gd cflags is ${GD_CFLAGS}])
    AC_MSG_NOTICE([gd libs is ${GD_LIBS}])
fi
AC_SUBST(GD_CFLAGS)
AC_SUBST(GD_LIBS)

if test x$with_libcurl != xno && test "x$using_cl" != "xyes"; then
    if test x$build = x$host; then
        AC_MSG_NOTICE([checking for libcurl])
        CFLAGS_BACKUP=$CFLAGS
        LDFLAGS_BACKUP=$LDFLAGS
        if test x$with_libcurl != xyes -a x$with_libcurl != xauto; then
            if test ! -d "$with_libcurl"; then
                AC_MSG_ERROR(["${with_libcurl}" is not directory])
            fi
            ADDED_CFLAGS="-I${with_libcurl}/include"
            ADDED_LIBS="-L${with_libcurl}/lib -lcurl"
        else
            if test x${prefix} != x -a x${prefix} != xNONE; then
                ADDED_CFLAGS="-I${prefix}/include"
                ADDED_LIBS="-L${prefix}/lib -lcurl"
            else
                ADDED_CFLAGS="-I/usr/local/include"
                ADDED_LIBS="-L/usr/local/lib -lcurl"
            fi
        fi
        CFLAGS="${CFLAGS} ${ADDED_CFLAGS}"
        LDFLAGS="${LDFLAGS} ${ADDED_LIBS}"
        AC_CHECK_HEADER([curl/curl.h],
                        [AC_CHECK_LIB([curl],
                                      [curl_easy_cleanup],
                                      [have_curl=yes
                                       LIBCURL_CFLAGS=$ADDED_CFLAGS
                                       LIBCURL_LIBS=$ADDED_LIBS],
                                      [have_curl=no])])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        if test x$have_curl != xyes -a ! -d "$with_libcurl"; then
            if test x${PKG_CONFIG} != x; then
                PKG_CHECK_MODULES(LIBCURL,
                                  [libcurl],
                                  [have_curl=yes],
                                  [have_curl=no])
            fi
        fi
    fi
    if test x$have_curl = xyes; then
        if test x${net} = x; then
            net="libcurl"
        else
            net="${net} libcurl"
        fi
        REQUIRES_PRIVATE="$REQUIRES_PRIVATE libcurl"
        LIBS_PRIVATE="$LIBS_PRIVATE $LIBCURL_LIBS"
        AC_DEFINE(HAVE_LIBCURL, 1, [whether libcurl is available])
    else
        if test x$with_libcurl != xauto; then
            AC_MSG_ERROR([unable to find libcurl])
        fi
    fi
    AC_MSG_NOTICE([libcurl cflags is ${LIBCURL_CFLAGS}])
    AC_MSG_NOTICE([libcurl libs is ${LIBCURL_LIBS}])
fi
AC_SUBST(LIBCURL_CFLAGS)
AC_SUBST(LIBCURL_LIBS)

if test x$with_winhttp != xno; then
    case "$host_os" in
    mingw* | cygwin* | msys*)
        AC_MSG_NOTICE([checking for WinHTTP])
        CFLAGS_BACKUP="$CFLAGS"
        LIBS_BACKUP="$LIBS"
        if test x$with_winhttp != xyes -a x$with_winhttp != xauto; then
            if test ! -d "$with_winhttp"; then
                AC_MSG_ERROR(["${with_winhttp}" is not directory])
            fi
            if test "x${using_cl}" = xyes; then
                ADDED_CFLAGS="-I${with_winhttp}\include"
                ADDED_LIBS="-lwinhttp"
            else
                ADDED_CFLAGS="-I${with_winhttp}/include"
                ADDED_LIBS="-L${with_winhttp}/lib -lwinhttp"
            fi
        else
            if test x${prefix} != x -a x${prefix} != xNONE; then
                if test "x${using_cl}" = xyes; then
                    ADDED_CFLAGS=
                    ADDED_LIBS="-lwinhttp"
                else
                    ADDED_CFLAGS="-I${prefix}/include"
                    ADDED_LIBS="-L${prefix}/lib -lwinhttp"
                fi
            else
                if test "x${using_cl}" = xyes; then
                    ADDED_CFLAGS=
                    ADDED_LIBS="-lwinhttp"
                else
                    ADDED_CFLAGS="-I/usr/local/include"
                    ADDED_LIBS="-L/usr/local/lib -lwinhttp"
                fi
            fi
        fi
        CFLAGS="${CFLAGS} ${ADDED_CFLAGS}"
        LIBS="${LIBS} ${ADDED_LIBS}"
        have_winhttp=yes
        AC_COMPILE_IFELSE(
            [AC_LANG_PROGRAM([[
              #include <windows.h>
              #include <winhttp.h>
            ]], [[]])],
            [AC_CHECK_LIB([winhttp],
                          [WinHttpOpen],
                          [],
                          [have_winhttp=no
                           AC_MSG_WARN([library winhttp is not avilable.])
                          ])],
            [have_winhttp=no
             AC_MSG_WARN([windows.h or winhttp.h is not avilable.])
            ])
        CFLAGS=$CFLAGS_BACKUP
        LIBS=$LIBS_BACKUP

        AS_IF([test "x$have_winhttp" = "xyes"],
              [AC_DEFINE([HAVE_WINHTTP],
                         [1],
                         [whether WinHTTP is available])
               if test x${net} = x; then
                   net="winhttp"
               else
                   net="${net} winhttp"
               fi
               WINHTTP_CFLAGS=$ADDED_CFLAGS
               WINHTTP_LIBS=$ADDED_LIBS],
              [])

        AC_MSG_NOTICE([winhttp cflags is ${WINHTTP_CFLAGS}])
        AC_MSG_NOTICE([winhttp libs is ${WINHTTP_LIBS}])
        ;;
    *)
        have_winhttp=no
        ;;
    esac
fi

AC_SUBST(WINHTTP_CFLAGS)
AC_SUBST(WINHTTP_LIBS)

if test x$with_jpeg != xno && test "x$using_cl" != "xyes"; then
    if test x$build = x$host; then
        AC_MSG_NOTICE([checking for jpeg])
        CFLAGS_BACKUP=$CFLAGS
        LDFLAGS_BACKUP=$LDFLAGS
        if test x$with_jpeg != xyes -a x$with_jpeg != xauto; then
            if test ! -d "$with_jpeg"; then
                AC_MSG_ERROR(["${with_jpeg}" is not directory])
            fi
            ADDED_CFLAGS="-I${with_jpeg}/include"
            ADDED_LIBS="-L${with_jpeg}/lib -ljpeg"
        else
            if test x${prefix} != x -a x${prefix} != xNONE; then
                ADDED_CFLAGS="-I${prefix}/include"
                ADDED_LIBS="-L${prefix}/lib -ljpeg"
            else
                ADDED_CFLAGS="-I/usr/local/include"
                ADDED_LIBS="-L/usr/local/lib -ljpeg"
            fi
        fi
        CFLAGS="${CFLAGS} ${ADDED_CFLAGS}"
        LDFLAGS="${LDFLAGS} ${ADDED_LIBS}"
        AC_CHECK_HEADER([jpeglib.h],
                        [AC_CHECK_LIB([jpeg],
                                      [jpeg_read_header],
                                      [have_jpeg=yes
                                       LIBJPEG_CFLAGS=$ADDED_CFLAGS
                                       LIBJPEG_LIBS=$ADDED_LIBS],
                                      [have_jpeg=no])])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        if test x$have_jpeg != xyes -a ! -d "$with_jpeg"; then
            if test x${PKG_CONFIG} != x; then
                PKG_CHECK_MODULES(LIBJPEG,
                                  [libjpeg],
                                  [have_jpeg=yes],
                                  [have_jpeg=no])
            fi
        fi
    fi
    if test x$have_jpeg = xyes; then
        AC_DEFINE(HAVE_JPEG, 1, [whether jpeg codec library is available])
        loaders="${loaders} jpeg"
        REQUIRES_PRIVATE="$REQUIRES_PRIVATE libjpeg"
        LIBS_PRIVATE="$LIBS_PRIVATE $LIBJPEG_LIBS"
        AC_MSG_NOTICE([jpeg cflags is ${LIBJPEG_CFLAGS}])
        AC_MSG_NOTICE([jpeg libs is ${LIBJPEG_LIBS}])
    else
        if test x$with_jpeg != xauto; then
            AC_MSG_ERROR([unable to find jpeg codec library])
        fi
        AC_MSG_NOTICE([libjpeg is not available])
    fi
fi
AC_SUBST(LIBJPEG_CFLAGS)
AC_SUBST(LIBJPEG_LIBS)

if test x$with_png != xno && test "x$using_cl" != "xyes"; then
    if test x$build = x$host; then
        AC_MSG_NOTICE([checking for png])
        CFLAGS_BACKUP=$CFLAGS
        LDFLAGS_BACKUP=$LDFLAGS
        if test x$with_png != xyes -a x$with_png != xauto; then
            if test ! -d "$with_png"; then
                AC_MSG_ERROR(["${with_png}" is not directory])
            fi
            ADDED_CFLAGS="-I${with_png}/include"
            ADDED_LIBS="-L${with_png}/lib -lpng"
        else
            if test x${prefix} != x -a x${prefix} != xNONE; then
                ADDED_CFLAGS="-I${prefix}/include"
                ADDED_LIBS="-L${prefix}/lib -lpng"
            else
                ADDED_CFLAGS="-I/usr/local/include"
                ADDED_LIBS="-L/usr/local/lib -lpng"
            fi
        fi
        CFLAGS="${CFLAGS} ${ADDED_CFLAGS}"
        LDFLAGS="${LDFLAGS} ${ADDED_LIBS}"
        AC_CHECK_HEADER([png.h],
                        [AC_CHECK_LIB([png],
                                      [png_check_sig],
                                      [have_png=yes
                                       LIBPNG_CFLAGS=$ADDED_CFLAGS
                                       LIBPNG_LIBS=$ADDED_LIBS],
                                      [have_png=no])])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        if test x$have_png != xyes -a ! -d "$with_png"; then
            if test x${PKG_CONFIG} != x; then
                PKG_CHECK_MODULES(LIBPNG,
                                  [libpng],
                                  [have_png=yes],
                                  [have_png=no])
            fi
        fi
    fi
    if test x$have_png = xyes; then
        CFLAGS="${CFLAGS} ${LIBPNG_CFLAGS}"
        LDFLAGS="${LDFLAGS} ${LIBPNG_LIBS}"
        AC_CHECK_DECLS([png_set_gray_1_2_4_to_8, png_set_expand_gray_1_2_4_to_8],
                       [], [], [ #include <png.h> ])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        AC_DEFINE(HAVE_LIBPNG, 1, [whether libpng is available])
        loaders="${loaders} png"
        REQUIRES_PRIVATE="$REQUIRES_PRIVATE libpng"
        LIBS_PRIVATE="$LIBS_PRIVATE $LIBPNG_LIBS"
        AC_MSG_NOTICE([libpng cflags is ${LIBPNG_CFLAGS}])
        AC_MSG_NOTICE([libpng libs is ${LIBPNG_LIBS}])
    else
        if test x$with_png != xauto; then
            AC_MSG_ERROR([unable to find libpng])
        fi
        AC_MSG_NOTICE([libpng is not available])
    fi
fi
AC_SUBST(LIBPNG_CFLAGS)
AC_SUBST(LIBPNG_LIBS)

if test x$with_winpthread != xno && test "x$using_cl" != "xyes"; then
    if test x$build = x$host; then
        AC_MSG_NOTICE([checking for winpthread])
        AC_SEARCH_LIBS([nanosleep],
                       [winpthread],
                       [AC_DEFINE([HAVE_NANOSLEEP],
                                  [1],
                                  [Define if nanosleep exists])
                        AC_DEFINE([HAVE_CLOCK],
                                  [1],
                                  [Define if clock exists])
                        AC_DEFINE([WITH_WINPTHREAD],
                                  [1],
                                  [Define if libwinpthread is available])
                        have_winpthread=yes
                       ],
                       [have_winpthread=yes])
        AS_IF([test "x$have_winpthread" = "xyes"],
              [AC_MSG_NOTICE([libpng cflags is ${LIBPNG_CFLAGS}])
               AC_MSG_NOTICE([libpng libs is ${LIBPNG_LIBS}])],
              [AC_MSG_NOTICE([libpng libs is not available])])
    fi
fi

if test x$with_wic != xno; then
    case "$host_os" in
    mingw* | cygwin* | msys* | msvc* )
        AC_MSG_NOTICE([checking for Windows Imaging Component (WIC)])
        CFLAGS_BACKUP="$CFLAGS"
        LIBS_BACKUP="$LIBS"
        if test x$with_wic != xyes -a x$with_wic != xauto; then
            if test ! -d "$with_wic"; then
                AC_MSG_ERROR(["${with_wic}" is not directory])
            fi
            if test "x${using_cl}" = xyes; then
                ADDED_CFLAGS="-I${with_wic}\include"
                ADDED_LIBS="-L${with_wic}/lib -lole32 -lwindowscodecs"
            else
                ADDED_CFLAGS="-I${with_wic}/include"
                ADDED_LIBS="-L${with_wic}/lib -lole32 -lwindowscodecs"
            fi
        else
            if test x${prefix} != x -a x${prefix} != xNONE; then
                if test "x${using_cl}" = xyes; then
                    ADDED_CFLAGS=
                    ADDED_LIBS="-lole32 -lwindowscodecs"
                else
                    ADDED_CFLAGS="-I${prefix}/include"
                    ADDED_LIBS="-L${prefix}/lib -lole32 -lwindowscodecs"
                fi
            else
                if test "x${using_cl}" = xyes; then
                    ADDED_CFLAGS=
                    ADDED_LIBS="-lole32 -lwindowscodecs"
                else
                    ADDED_CFLAGS="-I/usr/local/include"
                    ADDED_LIBS="-L/usr/local/lib -lole32 -lwindowscodecs"
                fi
            fi
        fi
        CFLAGS="${CFLAGS} ${ADDED_CFLAGS}"
        LIBS="${LIBS} ${ADDED_LIBS}"
        have_wic=yes
        AC_CHECK_HEADERS([windows.h wincodec.h],
                         [AC_CHECK_LIB([ole32],
                                       [main],
                                       [],
                                       [have_wic=no])
                          AC_CHECK_LIB([windowscodecs],
                                       [main],
                                       [],
                                       [have_wic=no])],
                         [have_wic=no])
        AS_IF([test "x$have_wic" = "xyes"],
              [AC_DEFINE([HAVE_WIC],
                         [1],
                         [whether Windows Imaging Component (WIC) is available])
               loaders="${loaders} wic"
               LIBS_PRIVATE="$LIBS_PRIVATE $WIC_LIBS"
               WIC_CFLAGS=$ADDED_CFLAGS
               WIC_LIBS=$ADDED_LIBS
               AC_MSG_NOTICE([WIC cflags is ${WIC_CFLAGS}])
               AC_MSG_NOTICE([WIC libs is ${WIC_LIBS}])],
              [AC_MSG_NOTICE([WIC is not available])])
        ;;
    *)
        have_wic=no
        ;;
    esac
    CFLAGS=$CFLAGS_BACKUP
    LIBS=$LIBS_BACKUP
fi
AC_SUBST(WIC_CFLAGS)
AC_SUBST(WIC_LIBS)

AC_SUBST(REQUIRES_PRIVATE)
AC_SUBST(LIBS_PRIVATE)

if test "x${enable_simd}" = xno; then
    message_simd=no
else
    case "${host_cpu}" in
    x86_64|amd64)
        have_sse2=yes
        ;;

    i*86)
        if x$ac_cv_header_emmintrin_h = xyes; then
            AC_MSG_CHECKING([whether the compiler supports -msse2 and <emmintrin.h>])
            saved_CFLAGS="$CFLAGS"
            CFLAGS="$CFLAGS -msse2"
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
                                #include <emmintrin.h>
                              ]], [[
                                __m128i a = _mm_setzero_si128();
                                (void)a;
                                return 0;
                              ]])],
                              [have_sse2=yes],
                              [have_sse2=no])
            CFLAGS="$saved_CFLAGS"
            AC_MSG_RESULT([$have_sse2])
            if test "x$have_sse2" = xyes; then
                AM_CFLAGS="$AM_CFLAGS -msse2"
            fi
        fi
        ;;

    aarch64|arm64)
        have_neon=yes
        ;;

    arm*|aarch32)
        if x$ac_cv_header_arm_neon_h = xyes; then
            AC_MSG_CHECKING([whether the compiler supports -mfpu=neon and <arm_neon.h>])
            saved_CFLAGS="$CFLAGS"
            CFLAGS="$CFLAGS -mfpu=neon"
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
                                #include <arm_neon.h>
                              ]], [[
                                int32x4_t v = vdupq_n_s32(0);
                                (void) v;
                                return 0;
                              ]])],
                              [have_neon=yes],
                              [have_neon=no])
            CFLAGS="$saved_CFLAGS"
            AC_MSG_RESULT([$have_neon])
            if test "x$have_neon" = xyes; then
                AM_CFLAGS="$AM_CFLAGS -mfpu=neon"
            fi
        fi
        ;;

    *)
        ;;

    esac
fi

if test "x$have_sse2" = xyes; then
    AC_DEFINE([HAVE_SSE2], [1], [Define if SSE2 is available])
    message_simd=sse2
fi
if test "x$have_neon" = xyes; then
    AC_DEFINE([HAVE_NEON], [1], [Define if NEON is available])
    message_neon=neon
fi

if test "x$enable_simd" = xyes; then
    if test "x$have_sse2" != xyes && test "x$have_neon" != xyes; then
        AC_MSG_ERROR([SIMD header is not found])
    fi
fi

if test "x${enable_wiccodec}" = xno; then
    message_wiccodec=no
else
    case "${host_os}" in
      mingw* | msvc* )
        have_wiccodec=yes
        if test x${have_wic} = xyes; then
            if test "x${using_cl}" = xyes; then
                WICCODEC_CFLAGS="$WIC_CFLAGS"
                WICCODEC_LIBS="$WIC_LIBS -ladvapi32 -luser32"
            else
                WICCODEC_CFLAGS="$WIC_CFLAGS"
                WICCODEC_LIBS="$WIC_LIBS"
            fi
        else
            AC_MSG_NOTICE([checking for Windows Imaging Component (WIC)])
            CFLAGS_BACKUP="$CFLAGS"
            LIBS_BACKUP="$LIBS"
            if test x${prefix} != x -a x${prefix} != xNONE; then
                if test "x${using_cl}" = xyes; then
                    ADDED_CFLAGS=
                    ADDED_LIBS="-lole32 -lwindowscodecs -ladvapi32 -luser32"
                else
                    ADDED_CFLAGS="-I${prefix}/include"
                    ADDED_LIBS="-L${prefix}/lib -lole32 -lwindowscodecs"
                fi
            else
                if test "x${using_cl}" = xyes; then
                    ADDED_CFLAGS=
                    ADDED_LIBS="-lole32 -lwindowscodecs -ladvapi32 -luser32"
                else
                    ADDED_CFLAGS="-I/usr/local/include"
                    ADDED_LIBS="-L/usr/local/lib -lole32 -lwindowscodecs"
                fi
            fi
            CFLAGS="${CFLAGS} ${ADDED_CFLAGS}"
            LIBS="${LIBS} ${ADDED_LIBS}"
            AC_CHECK_HEADERS([windows.h wincodec.h wincodecsdk.h],
                             [AC_CHECK_LIB([ole32],
                                           [main],
                                           [],
                                           [have_wiccodec=no])
                              AC_CHECK_LIB([windowscodecs],
                                           [main],
                                           [],
                                           [have_wiccodec=no])],
                             [have_wiccodec=no])
            WICCODEC_CFLAGS="$ADDED_CFLAGS"
            WICCODEC_LIBS="$ADDED_LIBS"
            CFLAGS=$CFLAGS_BACKUP
            LIBS=$CFLAGS_LIBS
        fi
        if test "x${have_wiccodec}" = xyes; then
            AC_DEFINE([HAVE_WICCODEC], [1],  [define 1 if wic codec support is enabled])
        fi
        ;;
      *)
        if test "x${enable_wiccodec}" = xyes; then
            AC_MSG_ERROR([--enable-wiccodec option is available only MinGW target])
        fi
        message_wiccodec=no
        ;;
    esac
fi
AC_SUBST(WICCODEC_CFLAGS)
AC_SUBST(WICCODEC_LIBS)

have_python=no
if test x$enable_python != xno; then
    AM_PATH_PYTHON([2.3], [have_python=yes], [have_python=no])
    if test x$enable_python = xyes -a x$have_python = xno; then
        AC_MSG_ERROR([python is not available])
    fi
fi
if test x$have_python = xyes; then
    message_python="$have_python: $pythondir"
else
    message_python=$have_python
fi

AC_DEFUN([LS_UPDATE_TIMESTAMP], [
          touch -c aclocal.m4 Makefile.in configure config.h.in \
                   converters/Makefile.in include/Makefile.in \
                   python/Makefile.in src/Makefile.in tools/Makefile.in \
                   wic/Makefile.in
          ])
LS_UPDATE_TIMESTAMP

AM_CONDITIONAL([HAVE_CURL], [test x$have_curl = xyes])
AM_CONDITIONAL([HAVE_WINHTTP], [test x$have_wic = xyes])
AM_CONDITIONAL([HAVE_JPEG], [test x$have_jpeg = xyes])
AM_CONDITIONAL([HAVE_PNG], [test x$have_png = xyes])
AM_CONDITIONAL([HAVE_WIC], [test x$have_wic = xyes])
AM_CONDITIONAL([HAVE_WICCODEC], [test x$have_wiccodec = xyes])
AM_CONDITIONAL([HAVE_PYTHON], [test x$have_python = xyes])

AC_CONFIG_FILES([Makefile
                 libsixel.pc
                 package.json.in
                 include/sixel.h
                 src/Makefile
                 include/Makefile
                 converters/Makefile
                 tools/Makefile
                 tools/libsixel-config
                 python/Makefile
                 wic/Makefile
                ])

AC_OUTPUT

echo ""
echo ""
echo "libsixel was configured as follows"
echo ""
echo "       Loader component    : $loaders"
echo "       Network access      : $net"
echo "       libwinpthread       : $have_winpthread"
echo "       pkg-config dir      : $pkgconfigdir"
echo "       Bash completion dir : $bashcompletiondir"
echo "       Zsh completion dir  : $zshcompletiondir"
echo "       WIC codec           : $message_wiccodec"
echo "       python bindings     : $message_python"
echo "       simd                : $message_simd"
echo "       gcov integration    : $enable_gcov"
echo "       debugging           : $enable_debug"
echo "       tests               : $enable_tests"
echo ""
echo ""
dnl :vim set et:
