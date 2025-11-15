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

AC_CHECK_TOOLS([CC], [clang clang.exe], [gcc])
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_REQUIRE_AUX_FILE([ar-lib])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])
LT_INIT
AC_CONFIG_SRCDIR([src/fromsixel.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AS_IF([test -z "$CC" && { test "x$MSYSTEM" = "xCLANG64" || test "x$MSYSTEM" = "xCLANGARM64"; }], [
  AC_CHECK_TOOLS([CC],  [clang clang.exe],  [gcc])
])
AC_PROG_AWK
AC_PROG_CC
AC_PROG_OBJC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PATH_PROG([UPDATE_MIME_DATABASE], [update-mime-database])

case "$host_os" in
  mingw*|msys*|cygwin*)
      build_windows=yes
      ;;
  *)
      build_windows=no
      ;;
esac
AM_CONDITIONAL([BUILDING_ON_WINDOWS],
               [test "x$build_windows" = "xyes"])

# Detect Microsoft Visual C compiler
case "$CC" in
  *cl|*cl.exe|*clang-cl*)
    using_cl=yes
    CFLAGS=`echo "$CFLAGS" | sed s/-g//g`
    CFLAGS="$CFLAGS -O2"
    LDFLAGS="$LDFLAGS -DEBUG"
    PKG_CONFIG=
    LD=link
    if test -z "${AR}"; then
        AR="${CONFIG_SHELL-$SHELL} $am_aux_dir/ar-lib lib"
    fi
    if test -z "${NM}"; then
        NM="dumpbin -symbols"
    fi
    STRIP=:
    RANLIB=:
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

AC_ARG_ENABLE([abort-trace],
              [AS_HELP_STRING([--enable-abort-trace],
                              [enable abort stack trace support for CLI tools
                               (default: auto)])],
              [enable_abort_trace="$enableval"],
              [enable_abort_trace="auto"])
AS_CASE([$enable_abort_trace],
        [yes|no|auto],
        [],
        [AC_MSG_ERROR([invalid value for --enable-abort-trace: $enable_abort_trace])])
AM_CONDITIONAL([HAVE_ABORT_TRACE],
               [test "x$enable_abort_trace" != "xno"])

AC_ARG_ENABLE([threads],
              [AS_HELP_STRING([--disable-threads],
                              [disable internal threadpool support (default: auto)])],
              [],
              [enable_threads=auto])
AM_CONDITIONAL([SIXEL_ENABLE_THREADS],
               [test "x$enable_threads" != "xno"])

AC_ARG_WITH([modeldir],
            [AS_HELP_STRING([--with-modeldir=DIR],
                            [install assessment models under DIR
                             @<:@default=${pkgdatadir}/models@:>@])],
            [],
            [with_modeldir=no])
if test "x$with_modeldir" = xno || test "x$with_modeldir" = xyes; then
  pkgdatadir="${datadir}/${PACKAGE_TARNAME}"
  model_root='${pkgdatadir}/models'
else
  model_root=$with_modeldir
fi
modeldir="${model_root}"
AC_SUBST(modeldir)
if test "x${prefix}" = xNONE; then
    prefix="${ac_default_prefix}"
    modeldir=`eval echo "${modeldir}"`
    modeldir=`eval echo "${modeldir}"`
    modeldir=`eval echo "${modeldir}"`
    prefix=NONE
else
    modeldir=`eval echo "${modeldir}"`
    modeldir=`eval echo "${modeldir}"`
    modeldir=`eval echo "${modeldir}"`
fi
AC_DEFINE_UNQUOTED([SIXEL_MODEL_DIR],
                   ["${modeldir}"],
                   [Base directory that holds libsixel assessment models])

AC_ARG_ENABLE([thumbnailer-command],
              [AS_HELP_STRING([--enable-thumbnailer-command],
                              [install the sixel thumbnailer command (default: auto)])],
              [enable_thumbnailer_command=$enableval],
              [enable_thumbnailer_command=auto])

AS_CASE([$enable_thumbnailer_command],
        [yes|no|auto],
        [],
        [AC_MSG_ERROR([invalid value for --enable-thumbnailer-command: $enable_thumbnailer_command])])

THREAD_CPPFLAGS=""
THREAD_CFLAGS=""
THREAD_LIBS=""
ABORTTRACE_LIBS=""
have_threads="no"
if test "x$enable_threads" != "xno"; then
  AC_DEFINE([SIXEL_ENABLE_THREADS],
            [1],
            [Define to 1 if internal threadpool support is enabled])
  THREAD_CPPFLAGS="-DSIXEL_ENABLE_THREADS=1"
  if test "x$build_windows" = "xyes"; then
    have_threads="yes"
  else
    AC_MSG_CHECKING([for pthreads])
    save_CFLAGS="$CFLAGS"
    save_LIBS="$LIBS"
    CFLAGS="$CFLAGS -pthread"
    LIBS="$LIBS -pthread"
    AC_LINK_IFELSE(
      [AC_LANG_PROGRAM([[#include <pthread.h>
static void *dummy(void *arg)
{
    (void)arg;
    return NULL;
}]],
                      [[pthread_t thread;
int rc;
rc = pthread_create(&thread, NULL, dummy, NULL);
if (rc == 0) {
    pthread_join(thread, NULL);
}
return rc;]])],
      [have_threads="yes"
       THREAD_CFLAGS="-pthread"
       THREAD_LIBS="-pthread"],
      [have_threads="no"])
    CFLAGS="$save_CFLAGS"
    LIBS="$save_LIBS"
    AC_MSG_RESULT([$have_threads])
    if test "x$have_threads" != "xyes"; then
      AC_CHECK_LIB([pthread], [pthread_create],
                   [have_threads="yes"
                    THREAD_LIBS="-lpthread"],
                   [])
    fi
    if test "x$have_threads" != "xyes"; then
      if test "x$enable_threads" = "xyes"; then
        AC_MSG_FAILURE([pthread support is required when threads are enabled])
      fi
    fi
  fi
else
  enable_threads=no
fi

AC_SUBST([THREAD_CPPFLAGS])
AC_SUBST([THREAD_CFLAGS])
AC_SUBST([THREAD_LIBS])

thumbnailer_command_mode=$enable_thumbnailer_command
thumbnailer_auto_installed=no
message_thumbnailer=no

if test x$enable_sixel2png = xno; then
  enable_thumbnailer_command=no
  message_thumbnailer=no
fi

if test x$enable_thumbnailer_command = xauto; then
  datadir_expanded=`eval echo "$datadir"`
  sixel_thumbnailer_install=no
  xdg_data_dirs=$XDG_DATA_DIRS
  if test -z "$xdg_data_dirs"; then
    xdg_data_dirs="/usr/local/share:/usr/share"
  fi
  old_IFS=$IFS
  IFS=:
  for xdg_dir in $xdg_data_dirs; do
    if test -n "$xdg_dir"; then
      sixel_thumbnailer_candidate="$xdg_dir/thumbnailers"
      if test -d "$sixel_thumbnailer_candidate"; then
        sixel_thumbnailer_install=yes
        break
      fi
    fi
  done
  IFS=$old_IFS
  if test x$sixel_thumbnailer_install = xno; then
    if test -n "$HOME"; then
      sixel_thumbnailer_candidate="$HOME/.local/share/thumbnailers"
      if test -d "$sixel_thumbnailer_candidate"; then
        sixel_thumbnailer_install=yes
      fi
    fi
  fi
  if test x$sixel_thumbnailer_install = xno; then
    sixel_thumbnailer_candidate="$datadir_expanded/thumbnailers"
    if test -d "$sixel_thumbnailer_candidate"; then
      sixel_thumbnailer_install=yes
    fi
  fi
  if test x$sixel_thumbnailer_install = xyes; then
    enable_thumbnailer_command=yes
    thumbnailer_auto_installed=yes
  else
    enable_thumbnailer_command=no
  fi
fi
AS_IF([test "x$enable_abort_trace" != "xno"], [
  AC_DEFINE([SIXEL_ENABLE_ABORT_TRACE],
            [1],
            [Define to 1 if CLI abort tracing is enabled])
  AC_CHECK_LIB([execinfo],
               [backtrace],
               [ABORTTRACE_LIBS="$ABORTTRACE_LIBS -lexecinfo"],
               [],
               [])
  ac_save_LIBS="$LIBS"
  LIBS="$LIBS $ABORTTRACE_LIBS"
  AC_CHECK_FUNCS([backtrace backtrace_symbols_fd],
                 [],
                 [],
                 [[#include <execinfo.h>]])
  LIBS="$ac_save_LIBS"
  AC_CHECK_LIB([dbghelp],
               [SymInitialize],
               [ABORTTRACE_LIBS="$ABORTTRACE_LIBS -ldbghelp"
                AC_DEFINE([HAVE_DBGHELP],
                          [1],
                          [Define to 1 if dbghelp is available])],
               [],
               [])
])
AC_SUBST([ABORTTRACE_LIBS])

if test x$message_thumbnailer = xno; then
  if test x$enable_thumbnailer_command = xyes; then
    if test x$thumbnailer_command_mode = xauto; then
      if test x$thumbnailer_auto_installed = xyes; then
        message_thumbnailer=yes
      else
        message_thumbnailer=yes
      fi
    else
      message_thumbnailer=yes
    fi
  elif test x$thumbnailer_command_mode = xauto; then
    message_thumbnailer=no
  fi
fi

AM_CONDITIONAL([WANT_THUMBNAILER], [test x$enable_thumbnailer_command = xyes])

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

AC_ARG_ENABLE([quicklook-extension],
              [AS_HELP_STRING([--enable-quicklook-extension],
                              [build the macOS Quick Look extension
                               (default: auto)])],
              [],
              [enable_quicklook_extension=auto])

AC_ARG_ENABLE([quicklook-preview],
              [AS_HELP_STRING([--enable-quicklook-preview],
                              [use macOS Quick Look for preview extraction
                               (default: auto)])],
              [enable_quicklook_preview=$enableval],
              [enable_quicklook_preview=auto])

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

# Auto-detect bash-completion directory with a v2 -> v1 fallback chain.
AC_ARG_WITH(bashcompletiondir,
            AS_HELP_STRING([--with-bashcompletiondir[=DIR]],
                           [Install bash completion to DIR (default: no)]),
                           [bashcompletiondir_request=$withval],
                           [bashcompletiondir_request=no])
AS_CASE([$bashcompletiondir_request],
        [yes|auto|''], [
            bashcompletiondir=
            for candidate in \
                "${datadir}/bash-completion/completions" \
                "/usr/share/bash-completion/completions" \
                "/usr/local/share/bash-completion/completions"
            do
                if test -d "$candidate"; then
                    bashcompletiondir=$candidate
                    break
                fi
            done
            if test -z "$bashcompletiondir"; then
                for candidate in \
                    "${sysconfdir}/bash_completion.d" \
                    "/etc/bash_completion.d" \
                    "/usr/local/etc/bash_completion.d"
                do
                    if test -d "$candidate"; then
                        bashcompletiondir=$candidate
                        break
                    fi
                done
            fi
            if test -z "$bashcompletiondir"; then
                bashcompletiondir=${datadir}/bash-completion/completions
            fi
        ],
        [no], [bashcompletiondir=],
        [bashcompletiondir=${bashcompletiondir_request}])
AS_IF([test -n "$bashcompletiondir"],
      [AC_MSG_NOTICE([bash-completion directory is ${bashcompletiondir}])],
      [AC_MSG_NOTICE([bash-completion installation disabled])])
AC_SUBST(bashcompletiondir)

# Auto-detect zsh completion directory by preferring vendor locations.
AC_ARG_WITH(zshcompletiondir,
            AS_HELP_STRING([--with-zshcompletiondir[=DIR]],
                           [Install zsh completion to DIR (default: no)]),
                           [zshcompletiondir_request=$withval],
                           [zshcompletiondir_request=no])
AS_CASE([$zshcompletiondir_request],
        [yes|auto|''], [
            zshcompletiondir=
            for candidate in \
                "${datadir}/zsh/vendor-completions" \
                "${datadir}/zsh/site-functions" \
                "/usr/share/zsh/vendor-completions" \
                "/usr/local/share/zsh/vendor-completions" \
                "/usr/share/zsh/site-functions" \
                "/usr/local/share/zsh/site-functions"
            do
                if test -d "$candidate"; then
                    zshcompletiondir=$candidate
                    break
                fi
            done
            if test -z "$zshcompletiondir"; then
                zshcompletiondir=${datadir}/zsh/vendor-completions
            fi
        ],
        [no], [zshcompletiondir=],
        [zshcompletiondir=${zshcompletiondir_request}])
AS_IF([test -n "$zshcompletiondir"],
      [AC_MSG_NOTICE([zsh-completion directory is ${zshcompletiondir}])],
      [AC_MSG_NOTICE([zsh-completion installation disabled])])
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

AC_ARG_ENABLE([register-dll],
              [AS_HELP_STRING([--enable-register-dll],
                              [register WIC codec DLL during install (default: no, admin rights required)])],
              [enable_register_dll=$enableval],
              [enable_register_dll=no])

AC_ARG_ENABLE([python],
              [AS_HELP_STRING([--enable-python],
                              [Python interface (default: no)])],
              [],
              [enable_python=no])

# Ruby bindings option
AC_ARG_ENABLE([ruby],
              [AS_HELP_STRING([--enable-ruby],
                              [Ruby interface (default: no)])],
              [],
              [enable_ruby=no])
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

AS_IF([test x$cross_compile != xyes],
      [PKG_PROG_PKG_CONFIG()
       AS_IF([test -z "$PKG_CONFIG"],
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
  LS_CHECK_CFLAG([-Wformat-nonliteral],
                 [AC_DEFINE(HAVE_DIAGNOSTIC_FORMAT_NONLITERAL,
                            [1],
                            [define 1 if GCC supports -Wformat-nonliteral])],
                 [],
                 [wformat_nonliteral])
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
                  stddef.h \
                  stdarg.h \
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
                  sys/wait.h \
                  spawn.h \
                  inttypes.h \
                  intrin.h \
                  emmintrin.h \
                  arm_neon.h \
                  dirent.h \
                  sys/sysctl.h \
                  float.h \
                  process.h \
                  execinfo.h \
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

AC_CHECK_FUNCS([assert \
                setmode \
                _setmode \
                fileno \
                _fileno \
                _mktemp \
                mkstemp \
                mktemp \
                fopen_s \
                fsync \
                _commit \
                _mkdir \
                mkdir \
                signal \
                strdup \
                strtoul \
                calloc \
                clearerr \
                stat \
                setjmp \
                longjmp \
                strerror \
                strerror_r \
                _strerror_r \
                _isatty \
                isatty \
                _open \
                _write \
                _close \
                sscanf_s \
                strncmp \
                ldiv \
                floor \
                pow \
                select \
                _fullpath \
                _realpath \
                realpath \
                sqrt \
                strchr \
                strerror \
                strstr \
                fchmod \
                strtol \
                tmpnam])

case "${host_os}" in
  mingw*)
    ;;
  *)
    AC_CHECK_FUNCS([clock nanosleep fork posix_spawnp])
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

if test "x$THREAD_LIBS" != x; then
    LIBS_PRIVATE="$LIBS_PRIVATE $THREAD_LIBS"
fi

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
    elif test x$with_coregraphics = xyes; then
        AC_MSG_ERROR([unable to find CoreGraphics])
    fi
fi
AC_SUBST(COREGRAPHICS_CFLAGS)
AC_SUBST(COREGRAPHICS_LIBS)

QUICKLOOK_LIBS=""
QUICKLOOK_CFLAGS=""
have_quicklook_preview=no
have_quicklook_thumbnailing=no
case $host_os in
  darwin*)
    if test x$enable_quicklook_preview != xno; then
        if test x$have_coregraphics != xyes; then
            if test x$enable_quicklook_preview = xyes; then
                AC_MSG_ERROR([Quick Look preview requires CoreGraphics support])
            fi
        else
            saved_LIBS="$LIBS"
            LIBS="$LIBS -framework QuickLook -framework CoreServices \
                  -framework ApplicationServices -framework CoreGraphics"
            AC_LINK_IFELSE(
                [AC_LANG_PROGRAM([[#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#include <CoreGraphics/CoreGraphics.h>]],
                                 [[CGSize size = CGSizeMake(1.0f, 1.0f);
CGImageRef image = QLThumbnailImageCreate(NULL, NULL, size, NULL);
if (image) {
    CGImageRelease(image);
}]])],
                [have_quicklook_preview=yes],
                [have_quicklook_preview=no])
            LIBS="$saved_LIBS"
            if test x$have_quicklook_preview = xyes; then
                AC_DEFINE(HAVE_QUICKLOOK, 1,
                          [whether macOS Quick Look is available])
                QUICKLOOK_LIBS="-framework QuickLook -framework CoreServices \
                                -framework ApplicationServices \
                                -framework CoreGraphics"
                QUICKLOOK_CFLAGS=""
                loaders="${loaders} quicklook"
                LIBS_PRIVATE="$LIBS_PRIVATE $QUICKLOOK_LIBS"
                AC_LANG_PUSH([Objective C])
                saved_LIBS_OBJC="$LIBS"
                added_OBJC_LIBS="-framework QuickLookThumbnailing \
                                   -framework UniformTypeIdentifiers \
                                   -framework Foundation"
                LIBS="$LIBS $added_OBJC_LIBS"
                AC_LINK_IFELSE(
                    [AC_LANG_PROGRAM([[#import <Foundation/Foundation.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>]],
                                     [[@autoreleasepool {
    NSURL *url = [NSURL fileURLWithPath:@"/"];
    CGSize size = CGSizeMake(16.0f, 16.0f);
    QLThumbnailGenerationRequest *request =
        [[QLThumbnailGenerationRequest alloc]
            initWithFileAtURL:url
                         size:size
                        scale:1.0f
          representationTypes:
              QLThumbnailGenerationRequestRepresentationTypeThumbnail];
    [[QLThumbnailGenerator sharedGenerator] cancelRequest:request];
    [request release];
} return 0;]])],
                    [have_quicklook_thumbnailing=yes],
                    [have_quicklook_thumbnailing=no])
                LIBS="$saved_LIBS_OBJC"
                AC_LANG_POP([Objective C])
                if test x$have_quicklook_thumbnailing = xyes; then
                    AC_DEFINE(HAVE_QUICKLOOK_THUMBNAILING, 1,
                              [whether macOS Quick Look Thumbnailing is available])
                    QUICKLOOK_LIBS="$QUICKLOOK_LIBS $added_OBJC_LIBS"
                fi
            elif test x$enable_quicklook_preview = xyes; then
                AC_MSG_ERROR([unable to find Quick Look framework])
            fi
        fi
    fi
    ;;
  *)
    if test x$enable_quicklook_preview = xyes; then
        AC_MSG_ERROR([Quick Look preview requires macOS])
    fi
    ;;
esac

AC_SUBST(QUICKLOOK_CFLAGS)
AC_SUBST(QUICKLOOK_LIBS)
AM_CONDITIONAL([HAVE_QUICKLOOK_THUMBNAILING],
               [test x$have_quicklook_thumbnailing = xyes])

CLIPBOARD_LIBS=""
CLIPBOARD_CFLAGS=""
have_clipboard_macos=no
have_clipboard_windows=no
have_clipboard_linux=no
case $host_os in
  darwin*)
    AC_LANG_PUSH([Objective C])
    saved_LIBS="$LIBS"
    LIBS="$LIBS -framework AppKit -framework Foundation"
    AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([[#import <AppKit/AppKit.h>]],
                         [[@autoreleasepool {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    return pb == nil;
}]])],
        [have_clipboard_macos=yes],
        [have_clipboard_macos=no])
    LIBS="$saved_LIBS"
    AC_LANG_POP([Objective C])
    if test x$have_clipboard_macos = xyes; then
        AC_DEFINE(HAVE_CLIPBOARD_MACOS, 1,
                  [whether macOS clipboard backend is available])
        CLIPBOARD_LIBS="-framework AppKit -framework Foundation"
    fi
    ;;
  mingw* | msys*)
    AC_CHECK_LIB([user32], [OpenClipboard],
        [have_clipboard_windows=yes
         AC_DEFINE(HAVE_CLIPBOARD_WINDOWS, 1,
                   [whether Windows clipboard backend is available])
         CLIPBOARD_LIBS="-luser32"],
        [have_clipboard_windows=no])
    ;;
  linux*)
    have_clipboard_linux=yes
    AC_DEFINE(HAVE_CLIPBOARD_LINUX, 1,
              [whether Linux clipboard backend is available])
    ;;
esac

AC_SUBST(CLIPBOARD_CFLAGS)
AC_SUBST(CLIPBOARD_LIBS)
AM_CONDITIONAL([HAVE_CLIPBOARD_MACOS],
               [test x$have_clipboard_macos = xyes])

APPLE_QUICKLOOK_EXTENSION_LIBS=""
APPLE_QUICKLOOK_REGISTER_LIBS=""
APPLE_QUICKLOOK_OBJCFLAGS="-fobjc-arc -fmodules"
have_quicklook_extension=no
message_quicklook_extension=no
case $host_os in
  darwin*)
    if test x$enable_quicklook_extension != xno; then
        quicklook_os_version_ok=yes
        quicklook_os_reason="requires macOS 10.15 or newer"
        macos_product_version=""
        if command -v sw_vers >/dev/null 2>&1; then
            macos_product_version=`sw_vers -productVersion 2>/dev/null`
        fi
        if test "x$macos_product_version" != x; then
            macos_version_major=`echo "$macos_product_version" | awk -F. '{print $1}'`
            macos_version_minor=`echo "$macos_product_version" | awk -F. '{print ($2 != "" ? $2 : 0)}'`
            case $macos_version_major in
              ''|*[!0-9]*)
                quicklook_os_version_ok=no
                quicklook_os_reason="unable to parse macOS version ($macos_product_version)"
                ;;
              10)
                case $macos_version_minor in ''|*[!0-9]*) macos_version_minor=0 ;; esac
                if test "$macos_version_minor" -lt 15; then
                    quicklook_os_version_ok=no
                fi
                ;;
              *)
                if test "$macos_version_major" -lt 10; then
                    quicklook_os_version_ok=no
                fi
                ;;
            esac
        else
            quicklook_os_version_ok=no
            quicklook_os_reason="unable to determine macOS version"
        fi

        if test x$quicklook_os_version_ok != xyes; then
            if test x$enable_quicklook_extension = xyes; then
                AC_MSG_ERROR([Quick Look extension requires macOS 10.15 or newer ($quicklook_os_reason)])
            else
                message_quicklook_extension="no (${quicklook_os_reason})"
            fi
        else
        AC_LANG_PUSH([Objective C])
        saved_LIBS="$LIBS"
        added_LIBS=
        added_LIBS="$added_LIBS -framework Foundation "
        added_LIBS="$added_LIBS -framework AppKit "
        added_LIBS="$added_LIBS -framework QuickLookUI "
        added_LIBS="$added_LIBS -framework QuickLookThumbnailing "
        added_LIBS="$added_LIBS -framework UniformTypeIdentifiers "
        added_LIBS="$added_LIBS -framework CoreGraphics "
        added_LIBS="$added_LIBS -framework ImageIO "
        LIBS="$LIBS $added_LIBS"
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM([[
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <QuickLookUI/QuickLookUI.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>]],
                             [[QLThumbnailReply *reply = nil; (void)reply;]])],
            [have_quicklook_extension=yes],
            [have_quicklook_extension=no])
        LIBS="$saved_LIBS"
        AC_LANG_POP([Objective C])
        if test x$have_quicklook_extension = xyes; then
            APPLE_QUICKLOOK_EXTENSION_LIBS="$added_LIBS"
            APPLE_QUICKLOOK_REGISTER_LIBS=
            APPLE_QUICKLOOK_REGISTER_LIBS="$APPLE_QUICKLOOK_REGISTER_LIBS -framework Foundation "
            APPLE_QUICKLOOK_REGISTER_LIBS="$APPLE_QUICKLOOK_REGISTER_LIBS -framework CoreServices "
            APPLE_QUICKLOOK_REGISTER_LIBS="$APPLE_QUICKLOOK_REGISTER_LIBS -framework UniformTypeIdentifiers "
            message_quicklook_extension=yes
        elif test x$enable_quicklook_extension = xyes; then
            AC_MSG_ERROR([Quick Look extension requested, but required frameworks were not found])
        else
            message_quicklook_extension="no (required frameworks unavailable)"
        fi
    fi
    fi
    ;;
  *)
    if test x$enable_quicklook_extension = xyes; then
        AC_MSG_ERROR([Quick Look extension requires macOS])
    fi
    ;;
esac

AC_SUBST(APPLE_QUICKLOOK_EXTENSION_LIBS)
AC_SUBST(APPLE_QUICKLOOK_REGISTER_LIBS)
AC_SUBST(APPLE_QUICKLOOK_OBJCFLAGS)
AM_CONDITIONAL([HAVE_APPLE_QUICKLOOK_EXTENSION], [test x$have_quicklook_extension = xyes])

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

AC_ARG_WITH([onnxruntime],
            [AS_HELP_STRING([--with-onnxruntime[=PATH]],
                            [use ONNX Runtime for LPIPS helper (default=auto)])],
            [],
            [with_onnxruntime=auto])

have_onnxruntime=no
AS_IF([test "x$with_onnxruntime" = "xno"],
      [have_onnxruntime=no],
      [
        if test "x$ONNXRUNTIME_CFLAGS$ONNXRUNTIME_LIBS" != x; then
            have_onnxruntime=yes
        fi
        if test "x$have_onnxruntime" != "xyes"; then
            PKG_CHECK_MODULES([ONNXRUNTIME],
                              [libonnxruntime],
                              [have_onnxruntime=yes],
                              [have_onnxruntime=no])
        fi
        if test "x$have_onnxruntime" != "xyes"; then
            if test "x$with_onnxruntime" != "xauto" \
                    -a "x$with_onnxruntime" != "xyes"; then
                if test -d "$with_onnxruntime"; then
                    ONNXRUNTIME_CFLAGS="-I$with_onnxruntime/include"
                    ONNXRUNTIME_LIBS="-L$with_onnxruntime/lib -lonnxruntime"
                    have_onnxruntime=yes
                else
                    AC_MSG_ERROR([--with-onnxruntime expects a directory])
                fi
            elif test "x$with_onnxruntime" = "xyes"; then
                AC_MSG_ERROR([libonnxruntime (ONNX Runtime) not found])
            fi
        fi
        if test "x$have_onnxruntime" = "xyes"; then
            save_CPPFLAGS="$CPPFLAGS"
            save_LIBS="$LIBS"
            CPPFLAGS="$CPPFLAGS $ONNXRUNTIME_CFLAGS"
            LIBS="$LIBS $ONNXRUNTIME_LIBS"
            AC_CHECK_HEADER([onnxruntime_c_api.h],
                            [],
                            [have_onnxruntime=no])
            AC_CHECK_LIB([onnxruntime],
                         [OrtGetApiBase],
                         [],
                         [have_onnxruntime=no])
            CPPFLAGS="$save_CPPFLAGS"
            LIBS="$save_LIBS"
        fi
      ])
AS_IF([test "x$have_onnxruntime" = "xyes"],
      [AC_DEFINE([HAVE_ONNXRUNTIME],
                 [1],
                 [Define if ONNX Runtime is available])],
      [ONNXRUNTIME_CFLAGS=
       ONNXRUNTIME_LIBS=])
AC_SUBST([ONNXRUNTIME_CFLAGS])
AC_SUBST([ONNXRUNTIME_LIBS])
AM_CONDITIONAL([HAVE_ONNXRUNTIME], [test "x$have_onnxruntime" = "xyes"])

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
    mingw* | cygwin* | msys* | msvc*)
        AC_MSG_NOTICE([checking for Windows Imaging Component (WIC)])
        CFLAGS_BACKUP="$CFLAGS"
        LIBS_BACKUP="$LIBS"
        if test x$with_wic != xyes -a x$with_wic != xauto; then
            if test ! -d "$with_wic"; then
                AC_MSG_ERROR(["${with_wic}" is not directory])
            fi
            if test "x${using_cl}" = xyes; then
                ADDED_CFLAGS="-I${with_wic}\include"
                ADDED_LIBS="-L${with_wic}/lib -lole32 -lwindowscodecs -lshlwapi -luuid"
            else
                ADDED_CFLAGS="-I${with_wic}/include"
                ADDED_LIBS="-L${with_wic}/lib -lole32 -lwindowscodecs"
            fi
        else
            if test x${prefix} != x -a x${prefix} != xNONE; then
                if test "x${using_cl}" = xyes; then
                    ADDED_CFLAGS=
                    ADDED_LIBS="-lole32 -lwindowscodecs -lshlwapi -luuid"
                else
                    ADDED_CFLAGS="-I${prefix}/include"
                    ADDED_LIBS="-L${prefix}/lib -lole32 -lwindowscodecs"
                fi
            else
                if test "x${using_cl}" = xyes; then
                    ADDED_CFLAGS=
                    ADDED_LIBS="-lole32 -lwindowscodecs -lshlwapi -luuid"
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
    message_simd=neon
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
      mingw* | msvc*)
        message_wiccodec=yes
        have_wiccodec=yes
        if test x${have_wic} = xyes; then
            if test "x${using_cl}" = xyes; then
                WICCODEC_CFLAGS="$WIC_CFLAGS"
                WICCODEC_LIBS="$WIC_LIBS -ladvapi32 -luser32"
            else
                WICCODEC_CFLAGS="$WIC_CFLAGS"
                WICCODEC_LIBS="$WIC_LIBS"
            fi
            AC_CHECK_HEADER([wincodecsdk.h],
                            [],
                            [message_wiccodec=no
                             have_wiccodec=no])
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
                                           [message_wiccodec=no
                                            have_wiccodec=no])
                              AC_CHECK_LIB([windowscodecs],
                                           [main],
                                           [],
                                           [message_wiccodec=no
                                            have_wiccodec=no])],
                             [message_wiccodec=no
                              have_wiccodec=no])
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
        have_wiccodec=no
        ;;
    esac
fi
if test "x${enable_wiccodec}" = xyes -a "x${have_wiccodec}" != xyes; then
    AC_MSG_ERROR([required WIC headers or libraries are not available])
fi
AC_SUBST(WICCODEC_CFLAGS)
AC_SUBST(WICCODEC_LIBS)

if test x$enable_register_dll = xyes; then
    if test "x${have_wiccodec}" != xyes; then
        AC_MSG_ERROR([--enable-register-dll requires WIC codec support])
    fi
fi

AM_CONDITIONAL([REGISTER_DLL], [test x$enable_register_dll = xyes])

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

message_ruby=no
RUBY=
RUBY_SITE_LIBDIR=
have_ruby=no
AS_IF([test "x$enable_ruby" != xno], [
  dnl Find Ruby explicitly first for robust behavior across environments
  AC_PATH_PROG([RUBY], [ruby], [])
  AS_IF([test "x$RUBY" != x], [have_ruby=yes], [have_ruby=no])

  dnl If AX_RUBY_EXT is available and Ruby is found, populate extra flags
  m4_ifdef([AX_RUBY_EXT], [
    AS_IF([test "x$have_ruby" = xyes], [AX_RUBY_EXT])
  ])

  AS_IF([test "x$have_ruby" = xno], [
    AS_IF([test "x$enable_ruby" = xyes], [
      AC_MSG_ERROR([ruby was not found, but --enable-ruby was specified])
    ])
  ], [
    AC_MSG_NOTICE([ruby found: $RUBY])
    dnl Derive Ruby site library directory. Prefer RbConfig query for robustness.
    RUBY_SITE_LIBDIR=`$RUBY -rrbconfig -e 'c = ::RbConfig.const_defined?(:CONFIG) ? ::RbConfig::CONFIG : {}; print( c.fetch(%q{sitelibdir}, nil) || c.fetch(%q{vendorlibdir}, nil) || c.fetch(%q{rubylibdir}, nil) || %q{} )'`
    AC_SUBST([RUBY_SITE_LIBDIR])
    AS_IF([test "x$RUBY_SITE_LIBDIR" != x], [
      message_ruby="yes: ${RUBY_SITE_LIBDIR}"
    ], [
      AS_IF([test "x$enable_ruby" = xyes], [AC_MSG_ERROR([failed to detect Ruby sitelibdir])])
    ])
  ])
])
AM_CONDITIONAL([HAVE_RUBY], [test "x$enable_ruby" != xno -a "x$have_ruby" = xyes -a "x$RUBY_SITE_LIBDIR" != x])

AC_ARG_VAR([BASH], [Path to the Bash shell for converter tests])
AC_PATH_PROG([BASH], [bash], [no])
AS_IF([test "x$BASH" = x -o "x$BASH" = xno], [
  AC_MSG_WARN([bash not found; converter tests will be skipped])
])
AM_CONDITIONAL([HAVE_BASH], [test "x$BASH" != x -a "x$BASH" != xno])

AC_DEFUN([LS_UPDATE_TIMESTAMP], [
          touch -c aclocal.m4 Makefile.in configure config.h.in \
                   converters/Makefile.in include/Makefile.in \
                   python/Makefile.in src/Makefile.in tools/Makefile.in \
                   assessment/Makefile.in wic/Makefile.in \
                   quicklook-extension/Makefile.in
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
                 assessment/Makefile
                 tools/Makefile
                 tools/libsixel-config
                 python/Makefile
                 ruby/Makefile
                 ruby/lib/libsixel/version_runtime.rb
                 quicklook-extension/Makefile
                 wic/Makefile
                ])

# Generate Ruby constants file from include/sixel.h when Ruby is enabled
AC_CONFIG_COMMANDS([rubyconstants], [
  if test "x$enable_ruby" != xno && test "x$RUBY" != x; then
    echo "Generating ruby/lib/libsixel/constants.rb from include/sixel.h"
    $RUBY tools/gen_ruby_constants.rb include/sixel.h ruby/lib/libsixel/constants.rb || exit 1
  fi
])

# Generate Quick Look Info.plist files only when the extension is enabled
AS_IF([test x$have_quicklook_extension = xyes], [
  AC_CONFIG_COMMANDS_PRE([
    test -d quicklook-extension/generated-resources || mkdir -p quicklook-extension/generated-resources
  ])
  AC_CONFIG_FILES([
    quicklook-extension/generated-resources/Host-Info.plist:quicklook-extension/Resources/Host-Info.plist.in
    quicklook-extension/generated-resources/Preview-Info.plist:quicklook-extension/Resources/Preview-Info.plist.in
    quicklook-extension/generated-resources/Thumbnail-Info.plist:quicklook-extension/Resources/Thumbnail-Info.plist.in
    quicklook-extension/generated-resources/Bridge-Info.plist:quicklook-extension/Resources/Bridge-Info.plist.in
  ])
])

AC_OUTPUT

if test x$have_wiccodec = xyes; then
    message_register_dll=$enable_register_dll
else
    message_register_dll=no
fi

AS_IF([test -n "$bashcompletiondir"],
      [message_bashcompletiondir=$bashcompletiondir],
      [message_bashcompletiondir=disabled])
AS_IF([test -n "$zshcompletiondir"],
      [message_zshcompletiondir=$zshcompletiondir],
      [message_zshcompletiondir=disabled])
AS_IF([test "x$have_onnxruntime" = "xyes"],
      [message_onnxruntime=yes],
      [message_onnxruntime=no])
message_threads=$have_threads

echo ""
echo ""
echo "libsixel was configured as follows"
echo ""
echo "       Loader component    : $loaders"
echo "       Network access      : $net"
echo "       libwinpthread       : $have_winpthread"
echo "       internal threads    : $message_threads"
echo "       pkg-config dir      : $pkgconfigdir"
echo "       Bash completion dir : $message_bashcompletiondir"
echo "       Zsh completion dir  : $message_zshcompletiondir"
echo "       QuickLook extension : $message_quicklook_extension"
echo "       WIC codec           : $message_wiccodec"
echo "       Register WIC DLL    : $message_register_dll"
echo "       Thumbnailer command : $message_thumbnailer"
echo "       ONNX Runtime        : $message_onnxruntime"
echo "       python bindings     : $message_python"
echo "       ruby bindings       : $message_ruby"
echo "       simd                : $message_simd"
echo "       gcov integration    : $enable_gcov"
echo "       debugging           : $enable_debug"
echo "       tests               : $enable_tests"
echo ""
echo ""
dnl :vim set et:
