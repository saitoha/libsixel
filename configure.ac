#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])
LT_PREREQ([2.4])
AC_INIT([libsixel], [1.11.0-pre], [saitoha@me.com])
AC_CONFIG_AUX_DIR([build-aux])
AC_REQUIRE_AUX_FILE([lso-tap-driver.sh.in])
LS_LT_CURRENT=1
LS_LT_REVISION=7
LS_LT_AGE=0
AC_SUBST([LS_LTVERSION], [$LS_LT_CURRENT:$LS_LT_REVISION:$LS_LT_AGE])
AC_SUBST([PACKAGE_DESCRIPTION],
         ["A lightweight, fast implementation of DEC SIXEL graphics codec"])

#AS_IF([test -z "$CC" && { test "x$MSYSTEM" = "xCLANG64" || test "x$MSYSTEM" = "xCLANGARM64"; }], [
#  AC_CHECK_TOOLS([CC],  [clang], [gcc])
#])
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_REQUIRE_AUX_FILE([ar-lib])
AM_MAINTAINER_MODE([disable])
AM_SILENT_RULES([yes])
LT_INIT

# Expose libtool runtime metadata to test environments so shell tests can
# execute the real binaries directly without probing stale build artifacts.
AC_SUBST([SIXEL_LIBTOOL_OBJDIR], [$objdir])
AC_SUBST([SIXEL_LIBTOOL_SHLIBPATH_VAR], [$shlibpath_var])
AC_SUBST([SIXEL_SHLIBPATH_OVERRIDES_RUNPATH],
         [$shlibpath_overrides_runpath])
AC_SUBST([SIXEL_LIBTOOL_BUILD_LIBS], [$enable_shared])
# The test harness prepends libtool output directories to the runtime search
# variable.  When libtool requests PATH, we are executing under a shell
# environment where PATH itself remains colon-separated (including MSYS2), so
# use ':' to keep the path list parseable by child processes.
case "$shlibpath_var" in
  PATH)
    SIXEL_LIBTOOL_PATH_SEPARATOR=':'
    ;;
  *)
    SIXEL_LIBTOOL_PATH_SEPARATOR=':'
    ;;
esac
AC_SUBST([SIXEL_LIBTOOL_PATH_SEPARATOR])

# Optional runtime path prefix for test commands.
#
# This is primarily used by MSYS2+MSVC jobs that depend on external DLLs
# provided by package managers (for example vcpkg). The value is prepended to
# the runtime shared-library search variable by test helpers.
AC_ARG_VAR([SIXEL_TEST_ADDITIOANL_PATH],
           [additional runtime library search path for test execution])
# Accept the correctly-spelled alias as an override while preserving backward
# compatibility with existing CI jobs and templates that use the legacy name.
AC_ARG_VAR([SIXEL_TEST_ADDITIONAL_PATH],
           [alias of SIXEL_TEST_ADDITIOANL_PATH])
AS_IF([test -n "$SIXEL_TEST_ADDITIONAL_PATH"], [
  SIXEL_TEST_ADDITIOANL_PATH="$SIXEL_TEST_ADDITIONAL_PATH"
])
AC_SUBST([SIXEL_TEST_ADDITIOANL_PATH])

AC_CONFIG_SRCDIR([src/fromsixel.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Check once whether the compiler accepts the builtin mismatch pragma.
AC_DEFUN([AC_CHECK_GCC_DIAG_WBUILTIN_DECL_MISMATCH], [
  AC_REQUIRE([AC_PROG_CC])
  AC_LANG_PUSH([C])
  AC_CACHE_CHECK([whether compiler accepts -Wbuiltin-declaration-mismatch pragma],
                 [ac_cv_c_pragma_wbuiltin_decl_mismatch],
                 [AC_COMPILE_IFELSE(
                    [AC_LANG_PROGRAM([[
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#endif
                    ]], [[return 0;]])],
                    [ac_cv_c_pragma_wbuiltin_decl_mismatch=yes],
                    [ac_cv_c_pragma_wbuiltin_decl_mismatch=no])])
  AC_LANG_POP([C])
])

# Run AC_CHECK_HEADERS checks in parallel using per-header directories.
# This mirrors AC_CHECK_HEADERS behavior while reducing overall configure time
# on systems with multiple CPUs.
AC_DEFUN([AC_CHECK_HEADERS_PARALLEL], [
  AC_REQUIRE([AX_COUNT_CPUS])
  AC_REQUIRE([AC_PROG_CC])
  AC_LANG_PUSH([C])
  m4_foreach_w([ac_check_header], [$1], [
    AH_TEMPLATE([HAVE_]AS_TR_CPP([ac_check_header]),
                [Define to 1 if you have the <]ac_check_header[> header file.])
  ])
  m4_ifval([$4], [ac_check_headers_parallel_includes="$4"],
           [ac_check_headers_parallel_includes=""])
  ac_check_headers_parallel_dir="ac-check-headers.$$"
  ac_check_headers_parallel_confdefs="$ac_pwd/confdefs.h"
  ac_check_headers_parallel_list="$1"
  ac_check_headers_parallel_todo=""
  ac_check_headers_parallel_cached=""
  ac_check_headers_parallel_active=0
  ac_check_headers_parallel_max="$CPU_COUNT"
  ac_check_headers_parallel_batch_pids=""

  # Limit parallelism by CPU_COUNT, falling back to 1 on invalid values.
  case "$ac_check_headers_parallel_max" in
    ''|*[!0123456789]*)
      ac_check_headers_parallel_max=1
      ;;
    0)
      ac_check_headers_parallel_max=1
      ;;
  esac

  # Split cached and non-cached headers to avoid needless checks.
  for ac_check_header in $ac_check_headers_parallel_list; do
    ac_check_headers_parallel_cache=\
`printf '%s\n' "$ac_check_header" | $as_tr_sh`
    eval "ac_check_headers_parallel_has_cache=\
\${ac_cv_header_$ac_check_headers_parallel_cache+set}"
    if test "x$ac_check_headers_parallel_has_cache" = "xset"; then
      ac_check_headers_parallel_cached=\
"$ac_check_headers_parallel_cached $ac_check_header"
    else
      ac_check_headers_parallel_todo=\
"$ac_check_headers_parallel_todo $ac_check_header"
    fi
  done

  # Run preprocessor tests in parallel in per-header directories to avoid
  # collisions on conftest files and logs.
  if test "x$ac_check_headers_parallel_todo" != "x"; then
    AS_MKDIR_P(["$ac_check_headers_parallel_dir"])
    for ac_check_header in $ac_check_headers_parallel_todo; do
      ac_check_headers_parallel_subdir=\
"$ac_check_headers_parallel_dir/$ac_check_header"
      AS_MKDIR_P(["$ac_check_headers_parallel_subdir"])
      (
        cd "$ac_check_headers_parallel_subdir" || exit 1
        cat "$ac_check_headers_parallel_confdefs" - <<_ACEOF \
> conftest.$ac_ext
/* end confdefs.h.  */
$ac_check_headers_parallel_includes
#include <$ac_check_header>
_ACEOF
        if ac_fn_c_try_cpp "$LINENO"; then
          printf '%s\n' yes > conftest.out
        else
          printf '%s\n' no > conftest.out
        fi
      ) &
      ac_check_headers_parallel_batch_pids=\
"$ac_check_headers_parallel_batch_pids $!"
      ac_check_headers_parallel_active=$((ac_check_headers_parallel_active + 1))
      if test "$ac_check_headers_parallel_active" -ge \
              "$ac_check_headers_parallel_max"; then
        for ac_check_headers_parallel_pid in \
$ac_check_headers_parallel_batch_pids; do
          wait "$ac_check_headers_parallel_pid"
        done
        ac_check_headers_parallel_batch_pids=""
        ac_check_headers_parallel_active=0
      fi
    done
  fi

  for ac_check_headers_parallel_pid in \
$ac_check_headers_parallel_batch_pids; do
    wait "$ac_check_headers_parallel_pid"
  done

  # Report results and update cache/defines serially for stable output.
  for ac_check_header in $ac_check_headers_parallel_list; do
    AC_MSG_CHECKING([for $ac_check_header])
    ac_check_headers_parallel_cache=\
`printf '%s\n' "$ac_check_header" | $as_tr_sh`
    eval "ac_check_headers_parallel_result=\
\${ac_cv_header_$ac_check_headers_parallel_cache-no}"
    if test "x$ac_check_headers_parallel_result" = "xno"; then
      ac_check_headers_parallel_subdir=\
"$ac_check_headers_parallel_dir/$ac_check_header"
      if test -f "$ac_check_headers_parallel_subdir/conftest.out"; then
        IFS= read -r ac_check_headers_parallel_result \
          < "$ac_check_headers_parallel_subdir/conftest.out"
      fi
    fi
    AC_MSG_RESULT([$ac_check_headers_parallel_result])
    AS_VAR_SET([ac_cv_header_$ac_check_headers_parallel_cache],
               [$ac_check_headers_parallel_result])
    if test "x$ac_check_headers_parallel_result" = "xyes"; then
      ac_check_headers_parallel_define=\
`printf '%s\n' "$ac_check_header" | $as_tr_cpp`
      AC_DEFINE_UNQUOTED([HAVE_$ac_check_headers_parallel_define], [1],
                         [Define to 1 if you have the <$ac_check_header> \
header file.])
      m4_ifval([$2], [$2], [:])
    else
      m4_ifval([$3], [$3], [:])
    fi
  done

  if test -d "$ac_check_headers_parallel_dir"; then
    rm -rf "$ac_check_headers_parallel_dir"
  fi
  AC_LANG_POP([C])
])

# Run AC_CHECK_FUNCS checks in parallel using per-function directories.
# This mirrors AC_CHECK_FUNCS behavior while reducing overall configure time
# on systems with multiple CPUs.
AC_DEFUN([AC_CHECK_FUNCS_PARALLEL], [
  AC_REQUIRE([AX_COUNT_CPUS])
  AC_REQUIRE([AC_CHECK_GCC_DIAG_WBUILTIN_DECL_MISMATCH])
  AC_REQUIRE([AC_PROG_CC])
  AC_LANG_PUSH([C])
  m4_foreach_w([ac_check_func], [$1], [
    AH_TEMPLATE([HAVE_]AS_TR_CPP([ac_check_func]),
                [Define to 1 if you have the `]ac_check_func[' function.])
  ])
  m4_ifval([$4], [ac_check_funcs_parallel_includes="$4"],
           [ac_check_funcs_parallel_includes=""])
  AC_CACHE_CHECK([for -fno-builtin],
                 [ac_cv_c_no_builtin],
                 [ac_check_funcs_parallel_save_CFLAGS=$CFLAGS
                  CFLAGS="$CFLAGS -fno-builtin"
                  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
                                    [ac_cv_c_no_builtin=yes],
                                    [ac_cv_c_no_builtin=no])
                  CFLAGS=$ac_check_funcs_parallel_save_CFLAGS])
  ac_check_funcs_parallel_cflags=""
  if test "x$ac_cv_c_no_builtin" = "xyes"; then
    ac_check_funcs_parallel_cflags="-fno-builtin"
  fi
  ac_check_funcs_parallel_dir="ac-check-funcs.$$"
  ac_check_funcs_parallel_confdefs="$ac_pwd/confdefs.h"
  ac_check_funcs_parallel_list="$1"
  ac_check_funcs_parallel_todo=""
  ac_check_funcs_parallel_cached=""
  ac_check_funcs_parallel_active=0
  ac_check_funcs_parallel_max="$CPU_COUNT"
  ac_check_funcs_parallel_batch_pids=""
  ac_check_funcs_parallel_diag_mismatch_pragma=""
  ac_check_funcs_parallel_diag_mismatch_pragma_end=""

  # Only emit the pragma when the compiler supports it.
  if test "x$ac_cv_c_pragma_wbuiltin_decl_mismatch" = "xyes"; then
    ac_check_funcs_parallel_diag_mismatch_pragma='
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#endif
'
    ac_check_funcs_parallel_diag_mismatch_pragma_end='
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
'
  fi

  # Limit parallelism by CPU_COUNT, falling back to 1 on invalid values.
  case "$ac_check_funcs_parallel_max" in
    ''|*[!0123456789]*)
      ac_check_funcs_parallel_max=1
      ;;
    0)
      ac_check_funcs_parallel_max=1
      ;;
  esac

  # Split cached and non-cached functions to avoid needless checks.
  for ac_func in $ac_check_funcs_parallel_list; do
    eval "ac_check_funcs_parallel_has_cache=\${ac_cv_func_$ac_func+set}"
    if test "x$ac_check_funcs_parallel_has_cache" = "xset"; then
      ac_check_funcs_parallel_cached="$ac_check_funcs_parallel_cached $ac_func"
    else
      ac_check_funcs_parallel_todo="$ac_check_funcs_parallel_todo $ac_func"
    fi
  done

  ac_check_funcs_parallel_save_CFLAGS=$CFLAGS
  if test "x$ac_check_funcs_parallel_cflags" != "x"; then
    CFLAGS="$CFLAGS $ac_check_funcs_parallel_cflags"
  fi

  # Run link tests in parallel in per-function directories to avoid
  # collisions on conftest files and logs.
  if test "x$ac_check_funcs_parallel_todo" != "x"; then
    AS_MKDIR_P(["$ac_check_funcs_parallel_dir"])
    for ac_func in $ac_check_funcs_parallel_todo; do
      ac_check_funcs_parallel_subdir=\
"$ac_check_funcs_parallel_dir/$ac_func"
      AS_MKDIR_P(["$ac_check_funcs_parallel_subdir"])
      (
        cd "$ac_check_funcs_parallel_subdir" || exit 1
        cat "$ac_check_funcs_parallel_confdefs" - <<_ACEOF \
> conftest.$ac_ext
/* end confdefs.h.  */
/* Define $ac_func to an innocuous variant, in case headers declare it. */
#define $ac_func innocuous_$ac_func

/* System header to define __stub macros and avoid conflicts. */
#include <limits.h>
$ac_check_funcs_parallel_includes
#undef $ac_func

/* Override any GCC internal prototype to avoid argument mismatch. */
#ifdef __cplusplus
extern "C"
#endif
$ac_check_funcs_parallel_diag_mismatch_pragma
char $ac_func (void);
$ac_check_funcs_parallel_diag_mismatch_pragma_end
/* The GNU C library defines this for functions which it implements to
   always fail with ENOSYS. */
#if defined __stub_$ac_func || defined __stub___$ac_func
choke me
#endif

int
main (void)
{
  return $ac_func ();
}
_ACEOF
        if ac_fn_c_try_link "$LINENO"; then
          printf '%s\n' yes > conftest.out
        else
          printf '%s\n' no > conftest.out
        fi
      ) &
      ac_check_funcs_parallel_batch_pids=\
"$ac_check_funcs_parallel_batch_pids $!"
      ac_check_funcs_parallel_active=$((ac_check_funcs_parallel_active + 1))
      if test "$ac_check_funcs_parallel_active" -ge \
              "$ac_check_funcs_parallel_max"; then
        for ac_check_funcs_parallel_pid in \
$ac_check_funcs_parallel_batch_pids; do
          wait "$ac_check_funcs_parallel_pid"
        done
        ac_check_funcs_parallel_batch_pids=""
        ac_check_funcs_parallel_active=0
      fi
    done
  fi

  for ac_check_funcs_parallel_pid in \
$ac_check_funcs_parallel_batch_pids; do
    wait "$ac_check_funcs_parallel_pid"
  done

  # Report results and update cache/defines serially for stable output.
  for ac_func in $ac_check_funcs_parallel_list; do
    AC_MSG_CHECKING([for $ac_func])
    eval "ac_check_funcs_parallel_result=\${ac_cv_func_$ac_func-no}"
    if test "x$ac_check_funcs_parallel_result" = "xno"; then
      ac_check_funcs_parallel_subdir=\
"$ac_check_funcs_parallel_dir/$ac_func"
      if test -f "$ac_check_funcs_parallel_subdir/conftest.out"; then
        IFS= read -r ac_check_funcs_parallel_result \
          < "$ac_check_funcs_parallel_subdir/conftest.out"
      fi
    fi
    AC_MSG_RESULT([$ac_check_funcs_parallel_result])
    AS_VAR_SET([ac_cv_func_$ac_func], [$ac_check_funcs_parallel_result])
    if test "x$ac_check_funcs_parallel_result" = "xyes"; then
      ac_check_funcs_parallel_define=\
`printf '%s\n' "$ac_func" | $as_tr_cpp`
      AC_DEFINE_UNQUOTED([HAVE_$ac_check_funcs_parallel_define], [1],
                         [Define to 1 if you have the `$ac_func' function.])
      m4_ifval([$2], [$2], [:])
    else
      m4_ifval([$3], [$3], [:])
    fi
  done

  CFLAGS=$ac_check_funcs_parallel_save_CFLAGS
  if test -d "$ac_check_funcs_parallel_dir"; then
    rm -rf "$ac_check_funcs_parallel_dir"
  fi
  AC_LANG_POP([C])
])

# Run config.status with explicit parallelism based on AX_COUNT_CPUS.
# The flow is:
# 1) Save the caller's --no-create choice.
# 2) Prevent AC_OUTPUT from running config.status itself.
# 3) Re-run config.status with --jobs=$CPU_COUNT when allowed.
AC_DEFUN([AC_OUTPUT_PARALLEL], [
  AC_REQUIRE([AX_COUNT_CPUS])
  ls_saved_no_create=$no_create
  no_create=yes
  AC_OUTPUT
  AS_IF([test "x$ls_saved_no_create" != xyes], [
    AS_IF([test "x$CPU_COUNT" = x || test "$CPU_COUNT" -lt 1 2>/dev/null], [
      CPU_COUNT=1
    ])
    AS_IF([test "$CPU_COUNT" -gt 1 2>/dev/null], [
      AC_CACHE_CHECK([whether config.status supports --jobs],
                     [ac_cv_config_status_jobs],
                     [ac_cv_config_status_jobs=no
                      ac_config_status_help=`$SHELL $CONFIG_STATUS --help \
                        2>/dev/null`
                      case " $ac_config_status_help " in
                        *" --jobs"*|*" --jobs="*)
                          ac_cv_config_status_jobs=yes
                          ;;
                      esac])
      AS_IF([test "x$ac_cv_config_status_jobs" = "xyes"],
            [ac_config_status_jobs="--jobs=$CPU_COUNT"],
            [ac_config_status_jobs=""])
    ], [
      ac_config_status_jobs=""
    ])
    $SHELL $CONFIG_STATUS $ac_config_status_jobs $ac_config_status_args \
      || as_fn_exit $?
  ])
  no_create=$ls_saved_no_create
])

AC_PROG_AWK

# Detect emscripten toolchain intent from either host triple or CC early.
# This must run before AC_PROG_CC so config.log records CC=emcc directly.
sixel_emcc_basename=`AS_BASENAME(["$CC"])`
sixel_emscripten_mode=no
case "$host_os" in
  emscripten*)
    sixel_emscripten_mode=yes
    ;;
esac
case "$host" in
  wasm32*-*-*emscripten*)
    sixel_emscripten_mode=yes
    ;;
esac
case "$sixel_emcc_basename" in
  emcc|emcc.py|emcc.bat)
    sixel_emscripten_mode=yes
    ;;
esac
if test "x$sixel_emscripten_mode" = "xyes" &&
   test "x$host_os" != "xemscripten"; then
  host_cpu=wasm32
  host_vendor=unknown
  host_os=emscripten
  host="$host_cpu-$host_vendor-$host_os"
  AC_MSG_NOTICE([forcing host triplet to $host from emscripten toolchain hints])
fi
if test "x$sixel_emscripten_mode" = "xyes" &&
   test "x$ac_env_CC_set" != "xset"; then
  CC=emcc
  AC_PATH_PROG([SIXEL_EMSCRIPTEN_CC], [emcc])
  if test -n "$SIXEL_EMSCRIPTEN_CC"; then
    CC="$SIXEL_EMSCRIPTEN_CC"
  fi
fi

AC_PROG_CC
AC_PROG_OBJC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PATH_PROG([UPDATE_MIME_DATABASE], [update-mime-database])

# Populate companion emscripten binutils when they are not explicitly provided.
if test "x$sixel_emscripten_mode" = "xyes"; then
  if test "x$ac_env_RANLIB_set" != "xset"; then
    AC_PATH_PROG([RANLIB], [emranlib], [ranlib])
  fi
  if test "x$ac_env_AR_set" != "xset"; then
    dnl emar is the canonical archiver wrapper; keep emer as compatibility fallback.
    AC_PATH_PROGS([AR], [emar emer], [ar])
  fi
  if test "x$ac_env_NM_set" != "xset"; then
    AC_PATH_PROGS([NM], [llvm-nm emnm], [nm])
  fi
  if test "x$ac_env_STRIP_set" != "xset"; then
    AC_PATH_PROG([STRIP], [emstrip], [strip])
  fi
fi

# Provide a conservative default parallelism for Makefiles when the caller
# did not already set -j/--jobs. The logic is:
# 1) Detect CPU_COUNT with AX_COUNT_CPUS.
# 2) Leave MAKEFLAGS untouched when -j or --jobs is present.
# 3) Otherwise, set LS_DEFAULT_MAKEFLAGS to "-j${CPU_COUNT}".
AX_COUNT_CPUS
LS_DEFAULT_MAKEFLAGS=""
case " $MAKEFLAGS " in
  *\ -j*|*\ --jobs*|*\ --jobs=* )
    ;;
  *)
    LS_DEFAULT_MAKEFLAGS="-j${CPU_COUNT}"
    ;;
esac
AC_SUBST([LS_DEFAULT_MAKEFLAGS])

AC_CACHE_CHECK([for date +%s.%N with sub-second resolution],
               [ac_cv_lso_date_epoch_nsec],
               [ac_cv_lso_date_epoch_nsec=no
                lso_date_sample1=`date +%s.%N 2>/dev/null`
                case $lso_date_sample1 in
                  *N*|*' '*|'') ;;
                  *.*)
                    sleep 0.02
                    lso_date_sample2=`date +%s.%N 2>/dev/null`
                    case $lso_date_sample2 in
                      *N*|*' '*|'') ;;
                      *.*)
                        if test "x$lso_date_sample1" != \
                                "x$lso_date_sample2"; then
                          lso_date_frac=`printf '%s\n' "$lso_date_sample1" \
                            | sed 's/^[^.]*\.//'`
                          lso_date_frac_len=`printf '%s\n' \
                            "$lso_date_frac" | "$AWK" '{print length($0)}'`
                          if test "$lso_date_frac_len" -ge 3; then
                            ac_cv_lso_date_epoch_nsec=yes
                          fi
                        fi
                        ;;
                    esac
                    ;;
                esac])

AS_IF([test "x$ac_cv_lso_date_epoch_nsec" = "xyes"],
      [LSO_TAP_DATE_CMD='date +%s.%N'],
      [LSO_TAP_DATE_CMD=''])
AC_SUBST([LSO_TAP_DATE_CMD])

# Emscripten cannot run lso-timeout, so skip building it there.
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

case "$host_os" in
  emscripten*)
      build_lso_timeout=no
      ;;
  *)
      build_lso_timeout=yes
      ;;
esac
AM_CONDITIONAL([BUILD_LSO_TIMEOUT],
               [test "x$build_lso_timeout" = "xyes"])

AS_CASE([$host_os],
        [darwin*],
        [CPPFLAGS="$CPPFLAGS -D_DARWIN_C_SOURCE=1"
         AC_DEFINE([_DARWIN_C_SOURCE],
                   [1],
                   [Define to 1 to enable Darwin extensions])])

# Detect Microsoft Visual C compiler and clang-cl
case "$CC" in
  *clang-cl*)
    using_cl=yes
    using_clangcl=yes
    for w in $CFLAGS; do
        test "$w" = "-g" || new_CFLAGS="$new $w"
    done
    CFLAGS="$new_CFLAGS -O2"
    LDFLAGS="$LDFLAGS -DEBUG"
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
  *cl|*cl.exe)
    using_cl=yes
    using_clangcl=no
    for w in $CFLAGS; do
        test "$w" = "-g" || new_CFLAGS="$new $w"
    done
    CFLAGS="$new_CFLAGS -O2"
    LDFLAGS="$LDFLAGS -DEBUG"
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
    using_clangcl=no
    ;;
esac
AM_CONDITIONAL([USING_CL], [test "x$using_cl" = "xyes"])

# Enable warnings-as-errors only in CI so local builds stay flexible.
AS_IF([test "x$CI" != "x"], [
  if test "x$using_cl" = "xyes"; then
    CFLAGS="$CFLAGS -WX"
    OBJCFLAGS="$OBJCFLAGS -WX"
  else
    CFLAGS="$CFLAGS -Werror"
    OBJCFLAGS="$OBJCFLAGS -Werror"
  fi
])

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

AC_ARG_ENABLE([sanitizer],
              [AS_HELP_STRING([--enable-sanitizer=@<:@no|address|undefined|address,undefined|memory|thread@:>@],
                              [enable sanitizer instrumentation and add
                               recommended compile/link flags
                               (default: no)])],
              [enable_sanitizer="$enableval"],
              [enable_sanitizer=no])
AS_CASE([$enable_sanitizer],
        [no],
        [sixel_sanitizer_cflags=""
         sixel_sanitizer_ldflags=""],
        [address],
        [sixel_sanitizer_cflags="-fsanitize=address -O1 -g -fno-omit-frame-pointer"
         sixel_sanitizer_ldflags="-fsanitize=address"],
        [undefined],
        [sixel_sanitizer_cflags="-fsanitize=undefined -O1 -g -fno-omit-frame-pointer"
         sixel_sanitizer_ldflags="-fsanitize=undefined"],
        [address,undefined],
        [sixel_sanitizer_cflags="-fsanitize=address,undefined -O1 -g -fno-omit-frame-pointer"
         sixel_sanitizer_ldflags="-fsanitize=address,undefined"],
        [memory],
        [sixel_sanitizer_cflags="-fsanitize=memory -O1 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls"
         sixel_sanitizer_ldflags="-fsanitize=memory"],
        [thread],
        [sixel_sanitizer_cflags="-fsanitize=thread -O1 -g -fno-omit-frame-pointer -fno-optimize-sibling-calls"
         sixel_sanitizer_ldflags="-fsanitize=thread"],
        [AC_MSG_ERROR([invalid value for --enable-sanitizer: $enable_sanitizer])])
AC_MSG_CHECKING([which sanitizer profile is enabled])
AC_MSG_RESULT([$enable_sanitizer])

AC_CACHE_CHECK(
  [whether ThreadSanitizer instrumentation is enabled],
  [ac_cv_sixel_tsan],
  [
   ac_save_CFLAGS="$CFLAGS"
   CFLAGS="$CFLAGS -Werror"
   AC_COMPILE_IFELSE(
     [AC_LANG_SOURCE([[
       #if defined(__has_feature)
       #if !__has_feature(thread_sanitizer)
       #error "TSan is not enabled"
       #endif
       #elif !defined(__SANITIZE_THREAD__)
       #error "TSan is not enabled"
       #endif
       int main(void) { return 0; }
     ]])],
     [ac_cv_sixel_tsan=yes],
     [ac_cv_sixel_tsan=no])
   CFLAGS="$ac_save_CFLAGS"
  ])
SIXEL_TSAN_BUILD="$ac_cv_sixel_tsan"
AC_SUBST([SIXEL_TSAN_BUILD])

AC_CACHE_CHECK(
  [whether MemorySanitizer instrumentation is enabled],
  [ac_cv_sixel_msan],
  [
   ac_save_CFLAGS="$CFLAGS"
   CFLAGS="$CFLAGS -Werror"
   AC_COMPILE_IFELSE(
     [AC_LANG_SOURCE([[
       #if defined(__has_feature)
       #if !__has_feature(memory_sanitizer)
       #error "MSan is not enabled"
       #endif
       #elif !defined(__SANITIZE_MEMORY__)
       #error "MSan is not enabled"
       #endif
       int main(void) { return 0; }
     ]])],
     [ac_cv_sixel_msan=yes],
     [ac_cv_sixel_msan=no])
   CFLAGS="$ac_save_CFLAGS"
  ])
SIXEL_MSAN_BUILD="$ac_cv_sixel_msan"
AC_SUBST([SIXEL_MSAN_BUILD])

# Options
AC_ARG_ENABLE([img2sixel],
              [AS_HELP_STRING([--disable-img2sixel],
                              [disable building the img2sixel utility (default: no)])],
              [],
              [enable_img2sixel=yes])
AM_CONDITIONAL(WANT_IMG2SIXEL, test x$enable_img2sixel != xno)
AS_IF([test "x$enable_img2sixel" != "xno"], [
  AC_DEFINE([HAVE_IMG2SIXEL],
            [1],
            [Define to 1 if img2sixel is enabled])
])

AC_ARG_ENABLE([sixel2png],
              [AS_HELP_STRING([--disable-sixel2png],
                              [disable building the sixel2png utility (default: no)])],
              [],
              [enable_sixel2png=yes])
AM_CONDITIONAL(WANT_SIXEL2PNG, test x$enable_sixel2png != xno)
AS_IF([test "x$enable_sixel2png" != "xno"], [
  AC_DEFINE([HAVE_SIXEL2PNG],
            [1],
            [Define to 1 if sixel2png is enabled])
])

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
AS_CASE([$host_os],
        [*emscripten*],
        [AS_IF([test "x$enable_abort_trace" = "xyes"],
               [AC_MSG_ERROR([--enable-abort-trace is not supported on emscripten])])
         enable_abort_trace="no"
         dnl Silence emcc pthreads/memory growth warning at link time.
         LDFLAGS="$LDFLAGS -Wno-pthreads-mem-growth"],
        [mingw*|*mingw*|*ucrt*|*win32*|*windows*],
        [AS_IF([test "x$enable_abort_trace" = "xyes"],
               [AC_MSG_ERROR([--enable-abort-trace is not supported on this Windows environment])])
         enable_abort_trace="no"])
AM_CONDITIONAL([HAVE_ABORT_TRACE],
               [test "x$enable_abort_trace" != "xno"])

AC_ARG_ENABLE([emscripten-retain_compiler_settings],
              [AS_HELP_STRING([--disable-emscripten-retain_compiler_settings],
                              [disable -sRETAIN_COMPILER_SETTINGS=1 on emscripten builds (default: enabled)])],
              [],
              [enable_emscripten_retain_compiler_settings=yes])

AC_ARG_ENABLE([emscripten-noderawfs],
              [AS_HELP_STRING([--disable-emscripten-noderawfs],
                              [disable -sNODERAWFS=1 on emscripten builds (default: enabled)])],
              [],
              [enable_emscripten_noderawfs=yes])

AC_ARG_WITH([emscripten-stack_size],
            [AS_HELP_STRING([--with-emscripten-stack_size=SIZE],
                            [set emscripten stack size (default: 786432)])],
            [with_emscripten_stack_size="$withval"],
            [with_emscripten_stack_size="786432"])

AC_ARG_WITH([emscripten-initial_memory],
            [AS_HELP_STRING([--with-emscripten-initial_memory=SIZE],
                            [set emscripten initial memory (default: 268435456)])],
            [with_emscripten_initial_memory="$withval"],
            [with_emscripten_initial_memory="268435456"])

AC_ARG_WITH([emscripten-environment],
            [AS_HELP_STRING([--with-emscripten-environment=LIST],
                            [set emscripten ENVIRONMENT list])],
            [with_emscripten_environment="$withval"],
            [with_emscripten_environment=""])

AC_ARG_ENABLE([emscripten-allow_memory_growth],
              [AS_HELP_STRING([--disable-emscripten-allow_memory_growth],
                              [disable -sALLOW_MEMORY_GROWTH=1 on emscripten builds (default: enabled)])],
              [],
              [enable_emscripten_allow_memory_growth=yes])

AC_ARG_ENABLE([emscripten-single_file],
              [AS_HELP_STRING([--disable-emscripten-single_file],
                              [enable -sSINGLE_FILE=1 on emscripten builds (default: disabled)])],
              [],
              [enable_emscripten_single_file=yes])

AC_ARG_ENABLE([emscripten-wasm_bigint],
              [AS_HELP_STRING([--disable-emscripten-wasm_bigint],
                              [enable -sWASM_BIGINT=1 on emscripten builds (default: disabled)])],
              [],
              [enable_emscripten_wasm_bigint=yes])

AC_ARG_ENABLE([emscripten-assertions],
              [AS_HELP_STRING([--disable-emscripten-assertions],
                              [add -sASSERTIONS=0 on emscripten builds (default: disabled)])],
              [],
              [enable_emscripten_assertions=yes])

EMSCRIPTEN_LDFLAGS=""
AS_CASE([$host_os],
        [emscripten*],
        [AS_IF([test "x$enable_emscripten_retain_compiler_settings" = "xyes"],
               [EMSCRIPTEN_LDFLAGS="$EMSCRIPTEN_LDFLAGS -sRETAIN_COMPILER_SETTINGS=1"])
         AS_IF([test "x$enable_emscripten_noderawfs" = "xyes"],
               [EMSCRIPTEN_LDFLAGS="$EMSCRIPTEN_LDFLAGS -sNODERAWFS=1"])
         AS_IF([test -n "$with_emscripten_stack_size"],
               [EMSCRIPTEN_LDFLAGS="$EMSCRIPTEN_LDFLAGS -sSTACK_SIZE=$with_emscripten_stack_size"])
         AS_IF([test -n "$with_emscripten_initial_memory"],
               [EMSCRIPTEN_LDFLAGS="$EMSCRIPTEN_LDFLAGS -sINITIAL_MEMORY=$with_emscripten_initial_memory"])
         AS_IF([test -n "$with_emscripten_environment"],
               [EMSCRIPTEN_LDFLAGS="$EMSCRIPTEN_LDFLAGS -sENVIRONMENT=$with_emscripten_environment"])
         AS_IF([test "x$enable_emscripten_allow_memory_growth" = "xyes"],
               [EMSCRIPTEN_LDFLAGS="$EMSCRIPTEN_LDFLAGS -sALLOW_MEMORY_GROWTH=1"])
         dnl These toggles intentionally follow the user-requested disable syntax.
         AS_IF([test "x$enable_emscripten_single_file" = "xno"],
               [EMSCRIPTEN_LDFLAGS="$EMSCRIPTEN_LDFLAGS -sSINGLE_FILE=1"])
         AS_IF([test "x$enable_emscripten_wasm_bigint" = "xno"],
               [EMSCRIPTEN_LDFLAGS="$EMSCRIPTEN_LDFLAGS -sWASM_BIGINT=1"])
         AS_IF([test "x$enable_emscripten_assertions" = "xno"],
               [EMSCRIPTEN_LDFLAGS="$EMSCRIPTEN_LDFLAGS -sASSERTIONS=0"])
         AS_IF([test -n "$EMSCRIPTEN_LDFLAGS"],
               [LDFLAGS="$LDFLAGS $EMSCRIPTEN_LDFLAGS"])])

AC_ARG_ENABLE([threads],
              [AS_HELP_STRING([--disable-threads],
                              [disable internal threadpool support (default: auto)])],
              [],
              [enable_threads=auto])
AM_CONDITIONAL([SIXEL_ENABLE_THREADS],
               [test "x$enable_threads" != "xno"])

AC_ARG_ENABLE([amalgamated-lib],
              [AS_HELP_STRING([--enable-amalgamated-lib],
                              [build libsixel from the amalgamated source
                               (default: no)])],
              [],
              [enable_amalgamated_lib=no])
AM_CONDITIONAL([WANT_AMALGAMATED_LIB],
               [test "x$enable_amalgamated_lib" = "xyes"])

AC_ARG_ENABLE([amalgamated-tools],
              [AS_HELP_STRING([--enable-amalgamated-tools],
                              [build CLI tools from the amalgamated source
                               (default: no)])],
              [],
              [enable_amalgamated_tools=no])
AM_CONDITIONAL([WANT_AMALGAMATED_TOOLS],
               [test "x$enable_amalgamated_tools" = "xyes"])

# This condition isolates "amalgamated tools only" builds so top-level make
# can serialize one-time amalgamation generation before phase2 starts.
AM_CONDITIONAL([WANT_AMALGAMATED_TOOLS_ONLY],
               [test "x$enable_amalgamated_tools" = "xyes" \
                && test "x$enable_amalgamated_lib" != "xyes"])

if test "x$enable_amalgamated_lib" = "xyes" \
   || test "x$enable_amalgamated_tools" = "xyes"; then
  want_amalgamated_sources=yes
else
  want_amalgamated_sources=no
fi
AM_CONDITIONAL([WANT_AMALGAMATED_SOURCES],
               [test "x$want_amalgamated_sources" = "xyes"])

AC_ARG_WITH([shebang-file],
            [AS_HELP_STRING([--with-shebang-file=PATH],
                            [prepend PATH contents to generated executables])],
            [with_shebang_file="$withval"],
            [with_shebang_file=""])

SIXEL_SHEBANG_FILE=""
if test -n "$with_shebang_file"; then
  AS_IF([test -f "$with_shebang_file"],
        [:],
        [AC_MSG_ERROR([unable to read --with-shebang-file: $with_shebang_file])])
  shebang_dir=`AS_DIRNAME(["$with_shebang_file"])`
  shebang_base=`AS_BASENAME(["$with_shebang_file"])`
  shebang_abs_dir=`cd "$shebang_dir" && pwd`
  SIXEL_SHEBANG_FILE="$shebang_abs_dir/$shebang_base"
fi

AC_SUBST([SIXEL_SHEBANG_FILE])
AC_SUBST([SIXEL_SHEBANG_HELPER],
         ["${top_srcdir}/build-aux/sixel-apply-shebang.sh"])
SIXEL_CHECK_TESTS=`$SHELL "$srcdir/build-aux/read-check-test-list.sh"     tests "$srcdir/tests/check-tests.list"`
AC_SUBST([SIXEL_CHECK_TESTS])
AM_CONDITIONAL([WITH_SHEBANG_FILE],
               [test -n "$SIXEL_SHEBANG_FILE"])

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

# Emscripten may emit a post-link optimization warning when debug
# information is enabled. In CI we globally enable -Werror, so downgrade
# this specific diagnostic to keep capability checks deterministic.
EMCC_WNO_ERROR_LIMITED_POSTLINK_OPT=""
case "$host_os" in
  emscripten*)
    AC_CACHE_CHECK([
      whether $CC supports -Wno-error=limited-postlink-optimizations
    ],
    [ac_cv_cc_wno_error_limited_postlink_optimizations],
    [ac_save_CFLAGS="$CFLAGS"
     CFLAGS="$CFLAGS -Werror -Wno-error=limited-postlink-optimizations"
     AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],
                       [ac_cv_cc_wno_error_limited_postlink_optimizations=yes],
                       [ac_cv_cc_wno_error_limited_postlink_optimizations=no])
     CFLAGS="$ac_save_CFLAGS"])
    AS_IF([test "x$ac_cv_cc_wno_error_limited_postlink_optimizations" = xyes],
          [EMCC_WNO_ERROR_LIMITED_POSTLINK_OPT="-Wno-error=limited-postlink-optimizations"])
    ;;
esac

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
    pthread_check_cflags="$CFLAGS -pthread"
    AS_IF([test -n "$EMCC_WNO_ERROR_LIMITED_POSTLINK_OPT"],
          [pthread_check_cflags="$pthread_check_cflags $EMCC_WNO_ERROR_LIMITED_POSTLINK_OPT"])
    CFLAGS="$pthread_check_cflags"
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

AC_ARG_ENABLE([gdk-pixbuf-loader],
              [AS_HELP_STRING([--enable-gdk-pixbuf-loader],
                              [Build gdk-pixbuf SIXEL loader])],
              [],
              [enable_gdk_pixbuf_loader=no])

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

AC_ARG_ENABLE([appkit],
              [AS_HELP_STRING([--disable-appkit],
                              [build the macOS clipboard backend without AppKit
                               (default: enable AppKit when available)])],
              [],
              [enable_appkit=yes])

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

AC_ARG_WITH([tiff],
            [AS_HELP_STRING([--with-tiff],
                            [whether to build with libtiff (default: auto)])],
            [],
            [with_tiff=auto])

AC_ARG_WITH([webp],
            [AS_HELP_STRING([--with-webp],
                            [whether to build with libwebp (default: auto)])],
            [],
            [with_webp=auto])

AC_ARG_WITH([librsvg],
            [AS_HELP_STRING([--with-librsvg],
                            [whether to build with librsvg and cairo (default: auto)])],
            [],
            [with_librsvg=auto])

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

AC_ARG_ENABLE([xsave-probe],
              [AS_HELP_STRING([--enable-xsave-probe],
                              [control _xgetbv probing for AVX detection
                               (default: auto)])],
              [],
              [enable_xsave_probe=auto])

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
                              [Python wheel package (default: no)])],
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
coverage_from_cflags=no
case " $CFLAGS " in
  *" --coverage "*|*" -fprofile-arcs "*|*" -ftest-coverage "*)
    coverage_from_cflags=yes
    ;;
esac
AS_IF([test x$enable_gcov = xno \
            -a x$coverage_from_cflags = xyes],
      [enable_gcov=yes
       AC_MSG_NOTICE([
         enabling gcov because coverage flags are present in CFLAGS
       ])])
AM_CONDITIONAL([COND_GCOV], [test x$enable_gcov != xno])
AM_CONDITIONAL([CFLAGS_HAVE_COVERAGE],
               [test x$coverage_from_cflags = xyes])

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
    # Keep the MSVC banner quiet during compilation and preprocessing.
    CPPFLAGS="-nologo $CPPFLAGS"
    LDFLAGS="-nologo $LDFLAGS"
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
  case "$host_os" in
    openbsd*)
      # Align Autotools with Meson: OpenBSD links shared objects with
      # --no-allow-shlib-undefined, and enabling -fno-emulated-tls can leave
      # __tls_get_addr unresolved when final tools link against libsixel.
      ;;
    *)
      LS_CHECK_CFLAG([-Bsymbolic],
                     [AM_CFLAGS="$AM_CFLAGS -Bsymbolic"
                      AC_DEFINE(HAVE_BSYMBOLIC,
                                [1],
                                [define 1 if GCC supports -Bsymbolic])],
                     [],
                     [bsymbolic])
      LS_CHECK_CFLAG([-fno-emulated-tls],
                     [AM_CFLAGS="$AM_CFLAGS -fno-emulated-tls"
                      AC_DEFINE(HAVE_FNO_EMULATED_TLS,
                                [1],
                                [define 1 if GCC supports -fno-emulated-tls])],
                     [],
                     [fno_emulated_tls])
      ;;
  esac
  AS_IF([test x$enable_debug = xyes],
        [LS_CHECK_CFLAG([-Werror], [AM_CFLAGS="$AM_CFLAGS -Werror"], [])])
fi

# In CI, CFLAGS includes -Werror. Keep emcc post-link optimization warnings
# non-fatal in normal build compilation as well as feature checks.
AS_IF([test -n "$EMCC_WNO_ERROR_LIMITED_POSTLINK_OPT" -a -n "$CI"],
      [AM_CFLAGS="$AM_CFLAGS $EMCC_WNO_ERROR_LIMITED_POSTLINK_OPT"])

AC_SUBST([AM_CFLAGS])

# Check math library
AC_CHECK_LIB([m], [floor])

# Checks for header files.
AC_CHECK_HEADERS_PARALLEL([assert.h \
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
                           sys/ttycom.h \
                           sys/wait.h \
                           spawn.h \
                           strings.h \
                           inttypes.h \
                           intrin.h \
                           emmintrin.h \
                           immintrin.h \
                           cpuid.h \
                           arm_neon.h \
                           dirent.h \
                           sys/sysctl.h \
                           float.h \
                           process.h \
                           execinfo.h \
                           windows.h \
                          ])

AC_MSG_CHECKING([for __builtin_cpu_supports])
AC_LINK_IFELSE([
    AC_LANG_PROGRAM([], [[
        #if !defined(__GNUC__) && !defined(__clang__)
        # error
        #endif
        __builtin_cpu_supports("sse2");
    ]])],
    [AC_DEFINE([HAVE_BUILTIN_CPU_SUPPORTS], [1],
               [Define if __builtin_cpu_supports is available])
     AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])])

AC_MSG_CHECKING([for __builtin_cpu_init])
AC_LINK_IFELSE([
    AC_LANG_PROGRAM([], [[
        #if !defined(__GNUC__) && !defined(__clang__)
        # error
        #endif
        __builtin_cpu_init();
    ]])],
    [AC_DEFINE([HAVE_BUILTIN_CPU_INIT], [1],
               [Define if __builtin_cpu_init is available])
     AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])])

AC_MSG_CHECKING([for __builtin_prefetch])
AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([], [[
        #if !defined(__GNUC__) && !defined(__clang__)
        # error
        #endif
        double distances[32];
        __builtin_prefetch(distances, 0, 3);
    ]])],
    [AC_DEFINE([HAVE_BUILTIN_PREFETCH], [1],
               [Define if __builtin_prefetch is available])
     AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])])

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

CLOCK_GETTIME_LIBS=""
LS_SAVED_LIBS=$LIBS
AC_SEARCH_LIBS([clock_gettime], [rt],
               [CLOCK_GETTIME_LIBS="$ac_cv_search_clock_gettime"])
AS_IF([test "x$CLOCK_GETTIME_LIBS" = "xnone required"],
      [CLOCK_GETTIME_LIBS=""])
AS_IF([test "x$CLOCK_GETTIME_LIBS" != "x" && \
       test "x$CLOCK_GETTIME_LIBS" != "xno"],
      [LIBS="$LIBS $CLOCK_GETTIME_LIBS"])

if test "x$using_cl" = "xyes"; then
    ac_cv_func_memcpy=yes
    ac_cv_func_memmove=yes
    ac_cv_func_memset=yes

    # Seed secure CRT symbols for cl/clang-cl builds where these APIs are
    # guaranteed by the Microsoft CRT family. This avoids redundant probes
    # while keeping MinGW/MSYS/Cygwin builds on the regular detection path.
    #
    # Seed policy table:
    #   +----------------+--------------------+
    #   | Cache variable | Seed value         |
    #   +----------------+--------------------+
    #   | ac_cv_func__*  | yes (MS CRT API)  |
    #   +----------------+--------------------+
    ac_cv_func__setmode=yes
    ac_cv_func__fileno=yes
    ac_cv_func__isatty=yes
    ac_cv_func__open=yes
    ac_cv_func__write=yes
    ac_cv_func__close=yes
    ac_cv_func__sopen_s=yes
fi

# Seed cache entries that are effectively fixed by host ABI families.
#
# Rationale:
# 1) fork() is unavailable on native Windows CRT environments.
# 2) Emscripten may expose fork() at link time in some SDK versions, but the
#    runtime path is ENOSYS, so shell/test flows must treat it as unavailable.
#
# Keeping this decision here avoids paying repeated probe costs while keeping
# behavior aligned with the existing platform policy below.
AS_CASE([$host_os],
        [*emscripten*|mingw*|*mingw*|*ucrt*|*win32*|*windows*],
        [ac_cv_func_fork=no])

AC_CHECK_FUNCS_PARALLEL([assert \
                         setmode \
                         _setmode \
                         memcpy \
                         memmove \
                         memset \
                         getopt \
                         getopt_long \
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
                         _chmod \
                         chmod \
                         signal \
                         strdup \
                         strcpy \
                         strcpy_s \
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
                         clock_gettime \
                         _sopen_s \
                         _open \
                         _write \
                         _close \
                         open \
                         sscanf_s \
                         strncmp \
                         tolower \
                         _stricmp \
                         _strcmpi \
                         strcasecmp \
                         ldiv \
                         floor \
                         pow \
                         select \
                         _fullpath \
                         _realpath \
                         _dupenv_s \
                         _putenv_s \
                         setenv \
                         realpath \
                         sqrt \
                         strchr \
                         strerror \
                         strstr \
                         fchmod \
                         strtol \
                         tmpnam \
                         gettimeofday \
                        ])
LIBS=$LS_SAVED_LIBS
AC_SUBST([CLOCK_GETTIME_LIBS])
AC_CHECK_FUNCS([_sopen_s], [], [], [[#include <io.h>
#include <share.h>]])
AC_CHECK_FUNCS([open], [], [], [[#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>]])

AS_IF([test "x$ac_cv_func_gettimeofday" = xyes], [
    AC_MSG_CHECKING([whether gettimeofday uses a void* timezone argument])
    AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([
            #include <sys/types.h>
            #include <sys/time.h>
        ], [
            int (*probe)(struct timeval *, void *) = gettimeofday;
            return probe(0, 0);
        ])
    ], [
        AC_MSG_RESULT([yes])
        AC_DEFINE([HAVE_GETTIMEOFDAY_TZ_VOID], [1],
                  [gettimeofday timezone argument is void*])
    ], [
        AC_MSG_RESULT([no])
        AC_MSG_CHECKING([
            whether gettimeofday uses a struct timezone* argument
        ])
        AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM([
                #include <sys/types.h>
                #include <sys/time.h>
            ], [
                int (*probe)(struct timeval *, struct timezone *) =
                    gettimeofday;
                return probe(0, 0);
            ])
        ], [
            AC_MSG_RESULT([yes])
            AC_DEFINE([HAVE_GETTIMEOFDAY_TZ_TIMEZONE], [1],
                      [gettimeofday timezone argument is struct timezone*])
        ], [
            AC_MSG_RESULT([no])
        ])
    ])
])

case "${host_os}" in
  mingw*)
    ;;
  emscripten*)
    AC_CHECK_HEADERS_PARALLEL([emscripten.h])
    AC_CHECK_FUNCS([emscripten_get_compiler_setting], [], [],
                   [[#include <emscripten.h>]])
    dnl fork() cache is pre-seeded above for emscripten hosts.
    AC_CHECK_FUNCS_PARALLEL([ \
        clock \
        nanosleep \
        usleep \
        fork \
        waitpid \
        posix_spawnp \
        execvp \
    ])
    ;;
  *)
    AC_CHECK_FUNCS_PARALLEL([ \
        clock \
        nanosleep \
        usleep \
        fork \
        waitpid \
        posix_spawnp \
        execvp \
    ])
    ;;
esac

have_freedesktop_thumbnailing_prereqs=no
AS_IF([test "x$ac_cv_header_unistd_h" = "xyes" &&
       test "x$ac_cv_header_sys_wait_h" = "xyes" &&
       test "x$ac_cv_func_fork" = "xyes" &&
       test "x$ac_cv_header_emscripten_h" != "xyes"],
      [have_freedesktop_thumbnailing_prereqs=yes])

AC_PATH_PROG([FILE_CMD], [file])
AC_PATH_PROG([CP_CMD], [cp])
sixel_file_mime_option=''
if test -n "$FILE_CMD"; then
    AC_MSG_CHECKING([for file(1) MIME option])
    sixel_file_mime_fixture="${srcdir}/tests/data/inputs/formats/rgba.png"
    for sixel_candidate in '--mime-type' '-I' '-i'; do
        sixel_file_mime_output=`"$FILE_CMD" "$sixel_candidate" \
            "$sixel_file_mime_fixture" 2>/dev/null | tr -d '\r'`
        case "$sixel_file_mime_output" in
            image/*|*:[[:space:]]image/*|*" image/"*)
                sixel_file_mime_option="$sixel_candidate"
                break
                ;;
        esac
    done
    if test -n "$sixel_file_mime_option"; then
        AC_MSG_RESULT([$sixel_file_mime_option])
    else
        AC_MSG_RESULT([none])
    fi
fi
AC_DEFINE_UNQUOTED([SIXEL_FILE_MIME_OPTION], ["$sixel_file_mime_option"],
                   [file(1) option used to obtain MIME type in thumbnailer
                    loader.])

AS_IF([test "x$have_freedesktop_thumbnailing_prereqs" = "xyes" &&
       test -n "$sixel_file_mime_option"],
      [AC_DEFINE([HAVE_FREEDESKTOP_THUMBNAILING], [1],
                 [Define to 1 when freedesktop thumbnailing loader
                  prerequisites are available.])])

if test x$ac_cv_header_windows_h = xyes; then
    AC_DEFINE([HAVE_WINDOWS_PROC], [1],
              [Define to 1 if Win32 process APIs are available.])
fi

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
LIBJPEG_PKG_NAME=""
have_webp="no"
LIBWEBP_PKG_NAME=""
have_png="no"
have_tiff="no"
LIBTIFF_PKG_NAME=""
have_winpthread="no"
have_wic="no"
have_coregraphics="no"
have_gdk_pixbuf2="no"
have_gdk_pixbuf_244="no"
have_gdk_pixbuf_pkg="no"
have_gdk_pixbuf_loader="no"
have_librsvg="no"
GDK_PIXBUF_QUERY_LOADERS=""
have_sse2=no
have_xgetbv_intrin=no
have_neon=no

REQUIRES_PRIVATE=""
LIBS_PRIVATE="$LIBS"

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


if test "x$THREAD_LIBS" != x; then
    LIBS_PRIVATE="$LIBS_PRIVATE $THREAD_LIBS"
fi

case "${host_cpu}" in
  x86_64|amd64|i*86)
    if test "x$enable_xsave_probe" != xno && \
       test "x$ac_cv_header_immintrin_h" = xyes; then
        AC_MSG_CHECKING([whether _xgetbv(0) builds without extra flags])
        AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM([[#include <immintrin.h>]],
                            [[unsigned long long xcr0;
                              xcr0 = _xgetbv(0);
                              return (int)xcr0;]])],
            [have_xgetbv_intrin=yes],
            [have_xgetbv_intrin=no])
        AC_MSG_RESULT([$have_xgetbv_intrin])
    fi
    ;;
  *)
    ;;
esac

if test "x$enable_xsave_probe" = xyes && \
   test "x$have_xgetbv_intrin" != xyes; then
    AC_MSG_ERROR([--enable-xsave-probe requested but _xgetbv is unavailable])
fi

if test "x$enable_xsave_probe" != xno && \
   test "x$have_xgetbv_intrin" = xyes; then
    AC_DEFINE([HAVE_XGETBV_INTRIN], [1],
              [Define to 1 if _xgetbv is available without extra flags])
    AC_DEFINE([ENABLE_XSAVE_PROBE], [1],
              [Define to 1 to enable XGETBV probing for AVX support])
fi

message_gdk_pixbuf_loader=
if test x$with_gdk_pixbuf2 != xno || \
   test x$enable_gdk_pixbuf_loader != xno; then
    AC_MSG_NOTICE([checking for gdk-pixbuf-2.0])
    if test x${PKG_CONFIG} != x; then
        PKG_CHECK_MODULES([GDK_PIXBUF],
                          [gdk-pixbuf-2.0 >= 2.28 gmodule-2.0],
                          [have_gdk_pixbuf_pkg=yes],
                          [have_gdk_pixbuf_pkg=no])
        if test x$have_gdk_pixbuf_pkg = xyes; then
            AC_MSG_NOTICE([gdk-pixbuf-2.0 cflags is ${GDK_PIXBUF_CFLAGS}])
            AC_MSG_NOTICE([gdk-pixbuf-2.0 libs is ${GDK_PIXBUF_LIBS}])
        elif test x$with_gdk_pixbuf2 = xyes || \
                test x$enable_gdk_pixbuf_loader = xyes; then
            AC_MSG_ERROR([gdk-pixbuf-2.0 not found via pkg-config])
        else
            AC_MSG_NOTICE([gdk-pixbuf-2.0 not found; disabling gdk-pixbuf2])
        fi
    else
        AC_MSG_NOTICE([pkg-config is not available])
        if test x${GDK_PIXBUF_CFLAGS} != x && \
           test x${GDK_PIXBUF_LIBS} != x; then
            have_gdk_pixbuf_pkg=yes
            AC_MSG_NOTICE([gdk-pixbuf-2.0 cflags is ${GDK_PIXBUF_CFLAGS}])
            AC_MSG_NOTICE([gdk-pixbuf-2.0 libs is ${GDK_PIXBUF_LIBS}])
        elif test x$with_gdk_pixbuf2 = xyes || \
                test x$enable_gdk_pixbuf_loader = xyes; then
            AC_MSG_ERROR([please set GDK_PIXBUF_CFLAGS and GDK_PIXBUF_LIBS, or install pkg-config.])
        else
            AC_MSG_NOTICE([gdk-pixbuf-2.0 not found; disabling gdk-pixbuf2])
        fi
    fi
fi

if test x$have_gdk_pixbuf_pkg = xyes ; then
    if test x$enable_gdk_pixbuf_loader != xno ; then
        have_gdk_pixbuf_loader=yes
        message_gdk_pixbuf_loader=yes
        if test x${PKG_CONFIG} != x; then
            PKG_CHECK_VAR([GDK_PIXBUF_BINARY_VERSION],
                          [gdk-pixbuf-2.0],
                          [gdk_pixbuf_binary_version],
                          [GDK_PIXBUF_BINARY_VERSION=])
            PKG_CHECK_VAR([GDK_PIXBUF_MODULEDIR],
                          [gdk-pixbuf-2.0],
                          [gdk_pixbuf_moduledir],
                          [GDK_PIXBUF_MODULEDIR=])
        fi
    fi

    if test x$with_gdk_pixbuf2 != xno ; then
        AC_DEFINE(HAVE_GDK_PIXBUF2, 1, [whether gdk-pixbuf2 is available])
        loaders="${loaders} gdk-pixbuf2"
        REQUIRES_PRIVATE="$REQUIRES_PRIVATE gdk-pixbuf-2.0"
        LIBS_PRIVATE="$LIBS_PRIVATE $GDK_PIXBUF_LIBS"
        if test x${PKG_CONFIG} != x; then
            PKG_CHECK_EXISTS([gdk-pixbuf-2.0 >= 2.44],
                             [have_gdk_pixbuf_244=yes],
                             [have_gdk_pixbuf_244=no])
        fi
    elif test x$with_gdk_pixbuf2 = xyes; then
        AC_MSG_ERROR([unable to find gdk-pixbuf-2.0])
    fi
fi

if test x$have_gdk_pixbuf_244 = xyes ; then
    AC_DEFINE(HAVE_GDK_PIXBUF_244, 1,
              [whether gdk-pixbuf2 2.44 or later is available])
fi

if test "x$GDK_PIXBUF_BINARY_VERSION" = x; then
    GDK_PIXBUF_BINARY_VERSION=2.10.0
fi

if test "x$GDK_PIXBUF_MODULEDIR" = x; then
    if test x${PKG_CONFIG} != x; then
        GDK_PIXBUF_MODULEDIR=`$PKG_CONFIG --variable=gdk_pixbuf_moduledir \
                                         gdk-pixbuf-2.0 2>/dev/null`
    fi
fi

if test "x$GDK_PIXBUF_MODULEDIR" = x; then
    GDK_PIXBUF_MODULEDIR='${libdir}/gdk-pixbuf-2.0/${GDK_PIXBUF_BINARY_VERSION}/loaders'
fi

if test "x$GDK_PIXBUF_MODULEDIR" != x; then
    AC_MSG_NOTICE([gdk-pixbuf moduledir is ${GDK_PIXBUF_MODULEDIR}])
fi

if test x$have_gdk_pixbuf_loader = xyes; then
    if test x${PKG_CONFIG} != x; then
        PKG_CHECK_VAR([GDK_PIXBUF_QUERY_LOADERS],
                      [gdk-pixbuf-2.0],
                      [gdk_pixbuf_query_loaders],
                      [],
                      [GDK_PIXBUF_QUERY_LOADERS=])
    fi
    if test "x$GDK_PIXBUF_QUERY_LOADERS" = x; then
        AC_PATH_PROG([GDK_PIXBUF_QUERY_LOADERS],
                     [gdk-pixbuf-query-loaders])
    fi
    if test "x$GDK_PIXBUF_QUERY_LOADERS" != x; then
        AC_MSG_NOTICE([gdk-pixbuf-query-loaders is ${GDK_PIXBUF_QUERY_LOADERS}])
    else
        AC_MSG_WARN([gdk-pixbuf-query-loaders not found; skipping cache updates during install])
    fi
fi

AM_CONDITIONAL([ENABLE_GDK_PIXBUF_LOADER],
               [test "x$enable_gdk_pixbuf_loader" = "xyes" -a \
                     "x$have_gdk_pixbuf_pkg" = "xyes"])

AC_SUBST(GDK_PIXBUF_BINARY_VERSION)
AC_SUBST(GDK_PIXBUF_MODULEDIR)
AC_SUBST(GDK_PIXBUF_CFLAGS)
AC_SUBST(GDK_PIXBUF_LIBS)
AC_SUBST(GDK_PIXBUF_QUERY_LOADERS)

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
have_clipboard_macos_appkit=no
have_clipboard_macos_carbon=no
have_clipboard_windows=no
have_clipboard_linux=no
case $host_os in
  darwin*)
    if test x$enable_appkit != xno; then
        AC_LANG_PUSH([Objective C])
        saved_LIBS="$LIBS"
        LIBS="$LIBS -framework AppKit -framework Foundation"
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM([[#import <AppKit/AppKit.h>]],
                             [[@autoreleasepool {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        return pb == nil;
    }]])],
            [have_clipboard_macos=yes
             have_clipboard_macos_appkit=yes],
            [have_clipboard_macos=no])
        LIBS="$saved_LIBS"
        AC_LANG_POP([Objective C])
        if test x$have_clipboard_macos_appkit = xyes; then
            AC_DEFINE(HAVE_APPKIT, 1,
                      [whether AppKit clipboard backend is available])
            CLIPBOARD_LIBS="-framework AppKit -framework Foundation"
        fi
    fi
    if test x$have_clipboard_macos != xyes; then
        AC_LANG_PUSH([C])
        saved_LIBS="$LIBS"
        LIBS="$LIBS -framework Carbon -framework CoreGraphics \
            -framework CoreFoundation"
        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM([[#include <ApplicationServices/ApplicationServices.h>]],
                             [[PasteboardRef pasteboard;
    OSStatus status;

    pasteboard = NULL;
    status = PasteboardCreate(kPasteboardClipboard, &pasteboard);
    if (status != noErr) {
        return 1;
    }
    status = PasteboardSynchronize(pasteboard);
    return status != noErr;]])],
            [have_clipboard_macos=yes
             have_clipboard_macos_carbon=yes
             CLIPBOARD_LIBS="-framework Carbon -framework CoreGraphics \
                -framework CoreFoundation"],
            [have_clipboard_macos=no])
        LIBS="$saved_LIBS"
        AC_LANG_POP([C])
    fi
    if test x$have_clipboard_macos = xyes; then
        AC_DEFINE(HAVE_CLIPBOARD_MACOS, 1,
                  [whether macOS clipboard backend is available])
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
AM_CONDITIONAL([HAVE_CLIPBOARD_MACOS_APPKIT],
               [test x$have_clipboard_macos_appkit = xyes])
AM_CONDITIONAL([HAVE_CLIPBOARD_MACOS_CARBON],
               [test x$have_clipboard_macos_carbon = xyes])

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

AMALGAMATION_NEEDS_OBJC=no
if test x$want_amalgamated_sources = xyes; then
    if test x$have_clipboard_macos_appkit = xyes \
        || test x$have_quicklook_thumbnailing = xyes; then
        AMALGAMATION_NEEDS_OBJC=yes
    fi
fi
AM_CONDITIONAL([AMALGAMATION_NEEDS_OBJC],
               [test x$AMALGAMATION_NEEDS_OBJC = xyes])

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
    dnl When building with MSVC from MSYS2, build/host differ even though
    dnl feature checks still need to run against the active compiler setup.
    if test x$build = x$host -o x$using_cl = xyes; then
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
    mingw* | cygwin* | msys* | msvc*)
        AC_MSG_NOTICE([checking for WinHTTP])
        CFLAGS_BACKUP="$CFLAGS"
        LIBS_BACKUP="$LIBS"
        if test x$with_winhttp != xyes -a x$with_winhttp != xauto; then
            if test ! -d "$with_winhttp"; then
                AC_MSG_ERROR(["${with_winhttp}" is not directory])
            fi
            if test "x${using_cl}" = xyes; then
                if test "x${using_clangcl}" = xyes; then
                    ADDED_CFLAGS="-I${with_winhttp}/include"
                    ADDED_LIBS="-L${with_winhttp}/lib -lwinhttp"
                else
                ADDED_CFLAGS="-I${with_winhttp}\\include"
                ADDED_LIBS="/LIBPATH:${with_winhttp}/lib winhttp.lib"
                fi
            else
                ADDED_CFLAGS="-I${with_winhttp}/include"
                ADDED_LIBS="-L${with_winhttp}/lib -lwinhttp"
            fi
        else
            if test x${prefix} != x -a x${prefix} != xNONE; then
                if test "x${using_cl}" = xyes; then
                    if test "x${using_clangcl}" = xyes; then
                        ADDED_CFLAGS="-I${prefix}/include"
                        ADDED_LIBS="-L${prefix}/lib -lwinhttp"
                    else
                        ADDED_CFLAGS=
                        ADDED_LIBS="winhttp.lib"
                    fi
                else
                    ADDED_CFLAGS="-I${prefix}/include"
                    ADDED_LIBS="-L${prefix}/lib -lwinhttp"
                fi
            else
                if test "x${using_cl}" = xyes; then
                    if test "x${using_clangcl}" = xyes; then
                        ADDED_CFLAGS="-I/usr/local/include"
                        ADDED_LIBS="-L/usr/local/lib -lwinhttp"
                    else
                        ADDED_CFLAGS=
                        ADDED_LIBS="winhttp.lib"
                    fi
                else
                    ADDED_CFLAGS="-I/usr/local/include"
                    ADDED_LIBS="-L/usr/local/lib -lwinhttp"
                fi
            fi
        fi
        CFLAGS="${CFLAGS} ${ADDED_CFLAGS}"
        LIBS="${LIBS} ${ADDED_LIBS}"
        have_winhttp=yes
        if test "x${using_cl}" = xyes -a "x${using_clangcl}" != xyes; then
            AC_LINK_IFELSE(
                [AC_LANG_PROGRAM([[#include <windows.h>
#include <winhttp.h>
                ]], [[
                  HINTERNET session;

                  session = WinHttpOpen(L"libsixel",
                                        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                        WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS,
                                        0);
                  if (session != NULL) {
                      WinHttpCloseHandle(session);
                  }
                  return 0;
                ]])],
                [],
                [have_winhttp=no
                 AC_MSG_WARN([winhttp is not available.])])
        else
            AC_COMPILE_IFELSE(
                [AC_LANG_PROGRAM([[#include <windows.h>
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
        fi
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

if test x$with_jpeg != xno; then
    if test x$host_os = xemscripten -a x$with_jpeg != xno; then
        dnl Validate Emscripten codec flag support before enabling jpeg.
        CFLAGS_BACKUP=$CFLAGS
        LDFLAGS_BACKUP=$LDFLAGS
        CFLAGS="$CFLAGS -sUSE_LIBJPEG=1"
        LDFLAGS="$LDFLAGS -sUSE_LIBJPEG=1"
        AC_MSG_CHECKING([whether emscripten accepts -sUSE_LIBJPEG=1])
        AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
                       [sixel_emscripten_use_libjpeg=yes],
                       [sixel_emscripten_use_libjpeg=no])
        AC_MSG_RESULT([$sixel_emscripten_use_libjpeg])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        if test x$sixel_emscripten_use_libjpeg = xyes; then
            have_jpeg=yes
            LIBJPEG_CFLAGS="-sUSE_LIBJPEG=1"
            LIBJPEG_LIBS="-sUSE_LIBJPEG=1"
            LIBJPEG_PKG_NAME=libjpeg
        elif test x$with_jpeg = xyes; then
            AC_MSG_ERROR([-sUSE_LIBJPEG=1 is not supported by this emscripten])
        fi
    fi
    dnl When building with MSVC from MSYS2, build/host differ even though
    dnl feature checks still need to run against the active compiler setup.
    if test x$build = x$host -o x$using_cl = xyes; then
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
                                      [have_jpeg=no])],
                        [have_jpeg=no])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        if test x$have_jpeg != xyes -a ! -d "$with_jpeg"; then
            if test x${PKG_CONFIG} != x; then
                PKG_CHECK_MODULES(LIBJPEG,
                                  [libjpeg],
                                  [have_jpeg=yes
                                   LIBJPEG_PKG_NAME=libjpeg],
                                  [have_jpeg=no])
                if test x$have_jpeg != xyes; then
                    PKG_CHECK_MODULES(LIBJPEG,
                                      [libjpeg-turbo],
                                      [have_jpeg=yes
                                       LIBJPEG_PKG_NAME=libjpeg-turbo],
                                      [have_jpeg=no])
                fi
            fi
        fi
    fi
    if test x$have_jpeg = xyes; then
        AC_DEFINE(HAVE_JPEG, 1, [whether jpeg codec library is available])
        loaders="${loaders} jpeg"
        if test x$LIBJPEG_PKG_NAME != x; then
            REQUIRES_PRIVATE="$REQUIRES_PRIVATE $LIBJPEG_PKG_NAME"
        else
            REQUIRES_PRIVATE="$REQUIRES_PRIVATE libjpeg"
        fi
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

if test x$with_png != xno; then
    if test x$host_os = xemscripten -a x$with_png != xno; then
        dnl Validate Emscripten codec flag support before enabling png.
        CFLAGS_BACKUP=$CFLAGS
        LDFLAGS_BACKUP=$LDFLAGS
        CFLAGS="$CFLAGS -sUSE_LIBPNG=1"
        LDFLAGS="$LDFLAGS -sUSE_LIBPNG=1"
        AC_MSG_CHECKING([whether emscripten accepts -sUSE_LIBPNG=1])
        AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
                       [sixel_emscripten_use_libpng=yes],
                       [sixel_emscripten_use_libpng=no])
        AC_MSG_RESULT([$sixel_emscripten_use_libpng])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        if test x$sixel_emscripten_use_libpng = xyes; then
            have_png=yes
            LIBPNG_CFLAGS="-sUSE_LIBPNG=1"
            LIBPNG_LIBS="-sUSE_LIBPNG=1"
            LIBPNG_PKG_NAME=libpng
        elif test x$with_png = xyes; then
            AC_MSG_ERROR([-sUSE_LIBPNG=1 is not supported by this emscripten])
        fi
    fi
    dnl When building with MSVC from MSYS2, build/host differ even though
    dnl feature checks still need to run against the active compiler setup.
    if test x$build = x$host -o x$using_cl = xyes; then
        AC_MSG_NOTICE([checking for png])
        CFLAGS_BACKUP=$CFLAGS
        LDFLAGS_BACKUP=$LDFLAGS
        PNG_LIBNAME=png
        if test "x$using_cl" = "xyes"; then
            PNG_LIBNAME=png16
        fi
        if test x$with_png != xyes -a x$with_png != xauto; then
            if test ! -d "$with_png"; then
                AC_MSG_ERROR(["${with_png}" is not directory])
            fi
            ADDED_CFLAGS="-I${with_png}/include"
            ADDED_LDFLAGS="-L${with_png}/lib"
        else
            if test x${prefix} != x -a x${prefix} != xNONE; then
                ADDED_CFLAGS="-I${prefix}/include"
                ADDED_LDFLAGS="-L${prefix}/lib"
            else
                ADDED_CFLAGS="-I/usr/local/include"
                ADDED_LDFLAGS="-L/usr/local/lib"
            fi
        fi
        CFLAGS="${CFLAGS} ${ADDED_CFLAGS}"
        LDFLAGS="${LDFLAGS} ${ADDED_LDFLAGS}"
        AC_CHECK_HEADER([png.h],
                        [AC_CHECK_LIB([png16],
                                      [png_sig_cmp],
                                      [have_png=yes
                                       LIBPNG_CFLAGS=$ADDED_CFLAGS
                                       LIBPNG_LIBS="${ADDED_LDFLAGS} -lpng16"
                                       LIBPNG_PKG_NAME=libpng16],
                                      [AC_CHECK_LIB([png],
                                                    [png_sig_cmp],
                                                    [have_png=yes
                                                     LIBPNG_CFLAGS=$ADDED_CFLAGS
                                                     LIBPNG_LIBS="${ADDED_LDFLAGS} -lpng"
                                                     LIBPNG_PKG_NAME=libpng],
                                                    [have_png=no])])])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        if test x$have_png != xyes -a ! -d "$with_png"; then
            if test x${PKG_CONFIG} != x; then
                PKG_CHECK_MODULES(LIBPNG,
                                  [libpng],
                                  [have_png=yes
                                   LIBPNG_PKG_NAME=libpng],
                                  [have_png=no])
                if test x$have_png != xyes; then
                    PKG_CHECK_MODULES(LIBPNG,
                                      [libpng16],
                                      [have_png=yes
                                       LIBPNG_PKG_NAME=libpng16],
                                      [have_png=no])
                fi
            fi
        fi
    fi
    if test x$have_png = xyes; then
        if test x$LIBPNG_PKG_NAME = x; then
            LIBPNG_PKG_NAME=${PNG_LIBNAME}
        fi
        CFLAGS="${CFLAGS} ${LIBPNG_CFLAGS}"
        LDFLAGS="${LDFLAGS} ${LIBPNG_LIBS}"
        AC_CHECK_DECLS([png_set_gray_1_2_4_to_8, png_set_expand_gray_1_2_4_to_8],
                       [], [], [ #include <png.h> ])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        AC_DEFINE(HAVE_LIBPNG, 1, [whether libpng is available])
        loaders="${loaders} png"
        REQUIRES_PRIVATE="$REQUIRES_PRIVATE ${LIBPNG_PKG_NAME}"
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

if test x$with_tiff != xno; then
    if test x$host_os = xemscripten -a x$with_tiff != xno; then
        dnl Validate Emscripten codec flag support before enabling tiff.
        CFLAGS_BACKUP=$CFLAGS
        LDFLAGS_BACKUP=$LDFLAGS
        CFLAGS="$CFLAGS -sUSE_LIBTIFF=1"
        LDFLAGS="$LDFLAGS -sUSE_LIBTIFF=1"
        AC_MSG_CHECKING([whether emscripten accepts -sUSE_LIBTIFF=1])
        AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
                       [sixel_emscripten_use_libtiff=yes],
                       [sixel_emscripten_use_libtiff=no])
        AC_MSG_RESULT([$sixel_emscripten_use_libtiff])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        if test x$sixel_emscripten_use_libtiff = xyes; then
            have_tiff=yes
            LIBTIFF_CFLAGS="-sUSE_LIBTIFF=1"
            LIBTIFF_LIBS="-sUSE_LIBTIFF=1"
            LIBTIFF_PKG_NAME=libtiff-4
        elif test x$with_tiff = xyes; then
            AC_MSG_ERROR([-sUSE_LIBTIFF=1 is not supported by this emscripten])
        fi
    fi
    if test x$build = x$host; then
        AC_MSG_NOTICE([checking for tiff])
        CFLAGS_BACKUP=$CFLAGS
        LDFLAGS_BACKUP=$LDFLAGS
        if test x$with_tiff != xyes -a x$with_tiff != xauto; then
            if test ! -d "$with_tiff"; then
                AC_MSG_ERROR(["${with_tiff}" is not directory])
            fi
            ADDED_CFLAGS="-I${with_tiff}/include"
            ADDED_LIBS="-L${with_tiff}/lib -ltiff"
        else
            if test x${prefix} != x -a x${prefix} != xNONE; then
                ADDED_CFLAGS="-I${prefix}/include"
                ADDED_LIBS="-L${prefix}/lib -ltiff"
            else
                ADDED_CFLAGS="-I/usr/local/include"
                ADDED_LIBS="-L/usr/local/lib -ltiff"
            fi
        fi
        CFLAGS="${CFLAGS} ${ADDED_CFLAGS}"
        LDFLAGS="${LDFLAGS} ${ADDED_LIBS}"
        AC_CHECK_HEADER([tiffio.h],
                        [AC_CHECK_LIB([tiff],
                                      [TIFFOpen],
                                      [have_tiff=yes
                                       LIBTIFF_CFLAGS=$ADDED_CFLAGS
                                       LIBTIFF_LIBS=$ADDED_LIBS],
                                      [have_tiff=no])],
                        [have_tiff=no])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        if test x$have_tiff != xyes -a ! -d "$with_tiff"; then
            if test x${PKG_CONFIG} != x; then
                PKG_CHECK_MODULES(LIBTIFF,
                                  [libtiff-4],
                                  [have_tiff=yes
                                   LIBTIFF_PKG_NAME=libtiff-4],
                                  [have_tiff=no])
            fi
        fi
    fi
    if test x$have_tiff = xyes; then
        AC_DEFINE(HAVE_LIBTIFF, 1, [whether libtiff is available])
        loaders="${loaders} tiff"
        if test x$LIBTIFF_PKG_NAME != x; then
            REQUIRES_PRIVATE="$REQUIRES_PRIVATE $LIBTIFF_PKG_NAME"
        else
            REQUIRES_PRIVATE="$REQUIRES_PRIVATE libtiff-4"
        fi
        LIBS_PRIVATE="$LIBS_PRIVATE $LIBTIFF_LIBS"
        AC_MSG_NOTICE([libtiff cflags is ${LIBTIFF_CFLAGS}])
        AC_MSG_NOTICE([libtiff libs is ${LIBTIFF_LIBS}])
    else
        if test x$with_tiff != xauto; then
            AC_MSG_ERROR([unable to find libtiff])
        fi
        AC_MSG_NOTICE([libtiff is not available])
    fi
fi
AC_SUBST(LIBTIFF_CFLAGS)
AC_SUBST(LIBTIFF_LIBS)

if test x$with_webp != xno; then
    if test x$host_os = xemscripten -a x$with_webp != xno; then
        dnl Validate Emscripten codec flag support before enabling webp.
        CFLAGS_BACKUP=$CFLAGS
        LDFLAGS_BACKUP=$LDFLAGS
        CFLAGS="$CFLAGS -sUSE_LIBWEBP=1"
        LDFLAGS="$LDFLAGS -sUSE_LIBWEBP=1"
        AC_MSG_CHECKING([whether emscripten accepts -sUSE_LIBWEBP=1])
        AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
                       [sixel_emscripten_use_libwebp=yes],
                       [sixel_emscripten_use_libwebp=no])
        AC_MSG_RESULT([$sixel_emscripten_use_libwebp])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        if test x$sixel_emscripten_use_libwebp = xyes; then
            have_webp=yes
            LIBWEBP_CFLAGS="-sUSE_LIBWEBP=1"
            LIBWEBP_LIBS="-sUSE_LIBWEBP=1"
            LIBWEBP_PKG_NAME=libwebp
        elif test x$with_webp = xyes; then
            AC_MSG_ERROR([-sUSE_LIBWEBP=1 is not supported by this emscripten])
        fi
    fi
    if test x$build = x$host; then
        AC_MSG_NOTICE([checking for webp])
        CFLAGS_BACKUP=$CFLAGS
        LDFLAGS_BACKUP=$LDFLAGS
        if test x$with_webp != xyes -a x$with_webp != xauto; then
            if test ! -d "$with_webp"; then
                AC_MSG_ERROR(["${with_webp}" is not directory])
            fi
            ADDED_CFLAGS="-I${with_webp}/include"
            ADDED_LIBS="-L${with_webp}/lib -lwebp -lwebpdemux"
        else
            if test x${prefix} != x -a x${prefix} != xNONE; then
                ADDED_CFLAGS="-I${prefix}/include"
                ADDED_LIBS="-L${prefix}/lib -lwebp -lwebpdemux"
            else
                ADDED_CFLAGS="-I/usr/local/include"
                ADDED_LIBS="-L/usr/local/lib -lwebp -lwebpdemux"
            fi
        fi
        CFLAGS="${CFLAGS} ${ADDED_CFLAGS}"
        LDFLAGS="${LDFLAGS} ${ADDED_LIBS}"
        AC_CHECK_HEADER([webp/decode.h],
                        [AC_CHECK_HEADER([webp/demux.h],
                                         [AC_CHECK_LIB([webp],
                                                       [WebPGetInfo],
                                                       [AC_CHECK_LIB([webpdemux],
                                                                     [WebPAnimDecoderGetInfo],
                                                                     [have_webp=yes
                                                                      LIBWEBP_CFLAGS=$ADDED_CFLAGS
                                                                      LIBWEBP_LIBS=$ADDED_LIBS],
                                                                     [have_webp=no])],
                                                       [have_webp=no])],
                                         [have_webp=no])],
                        [have_webp=no])
        CFLAGS=$CFLAGS_BACKUP
        LDFLAGS=$LDFLAGS_BACKUP
        if test x$have_webp != xyes -a ! -d "$with_webp"; then
            if test x${PKG_CONFIG} != x; then
                PKG_CHECK_MODULES(LIBWEBP,
                                  [libwebp libwebpdemux],
                                  [have_webp=yes
                                   LIBWEBP_PKG_NAME="libwebp libwebpdemux"],
                                  [have_webp=no])
            fi
        fi
    fi
    if test x$have_webp = xyes; then
        AC_DEFINE(HAVE_WEBP, 1, [whether libwebp is available])
        loaders="${loaders} webp"
        if test "x$LIBWEBP_PKG_NAME" != x; then
            REQUIRES_PRIVATE="$REQUIRES_PRIVATE $LIBWEBP_PKG_NAME"
        else
            REQUIRES_PRIVATE="$REQUIRES_PRIVATE libwebp libwebpdemux"
        fi
        LIBS_PRIVATE="$LIBS_PRIVATE $LIBWEBP_LIBS"
        AC_MSG_NOTICE([libwebp cflags is ${LIBWEBP_CFLAGS}])
        AC_MSG_NOTICE([libwebp libs is ${LIBWEBP_LIBS}])
    else
        if test x$with_webp != xauto; then
            AC_MSG_ERROR([unable to find libwebp])
        fi
        AC_MSG_NOTICE([libwebp is not available])
    fi
fi
AC_SUBST(LIBWEBP_CFLAGS)
AC_SUBST(LIBWEBP_LIBS)


if test x$with_librsvg != xno; then
    AC_MSG_NOTICE([checking for librsvg + cairo])
    if test x${PKG_CONFIG} != x; then
        PKG_CHECK_MODULES([LIBRSVG],
                          [librsvg-2.0 >= 2.46 cairo],
                          [have_librsvg=yes],
                          [have_librsvg=no])
    else
        have_librsvg=no
    fi

    if test x$have_librsvg = xyes; then
        AC_DEFINE([HAVE_LIBRSVG], [1],
                  [whether librsvg and cairo are available])
        loaders="${loaders} librsvg"
        REQUIRES_PRIVATE="$REQUIRES_PRIVATE librsvg-2.0 cairo"
        LIBS_PRIVATE="$LIBS_PRIVATE $LIBRSVG_LIBS"
        AC_MSG_NOTICE([librsvg cflags is ${LIBRSVG_CFLAGS}])
        AC_MSG_NOTICE([librsvg libs is ${LIBRSVG_LIBS}])
    else
        if test x$with_librsvg != xauto; then
            AC_MSG_ERROR([unable to find librsvg and cairo])
        fi
        AC_MSG_NOTICE([librsvg is not available])
    fi
fi
AC_SUBST(LIBRSVG_CFLAGS)
AC_SUBST(LIBRSVG_LIBS)

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
                if test "x${using_clangcl}" = xyes; then
                    ADDED_CFLAGS="-I${with_wic}/include"
                    ADDED_LIBS="-L${with_wic}/lib -lole32 -lwindowscodecs"
                else
                    ADDED_CFLAGS="-I${with_wic}\\include"
                    ADDED_LIBS="/LIBPATH:${with_wic}/lib windowscodecs.lib \
ole32.lib shlwapi.lib uuid.lib"
                fi
            else
                ADDED_CFLAGS="-I${with_wic}/include"
                ADDED_LIBS="-L${with_wic}/lib -lole32 -lwindowscodecs"
            fi
        else
            if test x${prefix} != x -a x${prefix} != xNONE; then
                if test "x${using_cl}" = xyes; then
                    if test "x${using_clangcl}" = xyes; then
                        ADDED_CFLAGS="-I${prefix}/include"
                        ADDED_LIBS="-L${prefix}/lib -lole32 -lwindowscodecs"
                    else
                        ADDED_CFLAGS=
                        ADDED_LIBS="windowscodecs.lib ole32.lib shlwapi.lib \
uuid.lib"
                    fi
                else
                    ADDED_CFLAGS="-I${prefix}/include"
                    ADDED_LIBS="-L${prefix}/lib -lole32 -lwindowscodecs"
                fi
            else
                if test "x${using_cl}" = xyes; then
                    if test "x${using_clangcl}" = xyes; then
                        ADDED_CFLAGS="-I/usr/local/include"
                        ADDED_LIBS="-L/usr/local/lib -lole32 -lwindowscodecs"
                    else
                        ADDED_CFLAGS=
                        ADDED_LIBS="windowscodecs.lib ole32.lib shlwapi.lib \
uuid.lib"
                    fi
                else
                    ADDED_CFLAGS="-I/usr/local/include"
                    ADDED_LIBS="-L/usr/local/lib -lole32 -lwindowscodecs"
                fi
            fi
        fi
        CFLAGS="${CFLAGS} ${ADDED_CFLAGS}"
        LIBS="${LIBS} ${ADDED_LIBS}"
        have_wic=yes
        if test "x${using_cl}" = xyes -a "x${using_clangcl}" != xyes; then
            AC_LINK_IFELSE(
                [AC_LANG_PROGRAM([[#include <windows.h>
 #include <wincodec.h>
                ]], [[
                  IWICImagingFactory *factory;

                  factory = NULL;
                  if (FAILED(CoInitialize(NULL))) {
                      return 1;
                  }
                  if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory,
                                              NULL,
                                              CLSCTX_INPROC_SERVER,
                                              &IID_IWICImagingFactory,
                                              (LPVOID *)&factory))) {
                      CoUninitialize();
                      return 1;
                  }
                  if (factory != NULL) {
                      IWICImagingFactory_Release(factory);
                  }
                  CoUninitialize();
                  return 0;
                ]])],
                [],
                [have_wic=no])
        else
            AC_CHECK_HEADERS_PARALLEL([windows.h wincodec.h],
                             [AC_CHECK_LIB([ole32],
                                           [main],
                                           [],
                                           [have_wic=no])
                              AC_CHECK_LIB([windowscodecs],
                                           [main],
                                           [],
                                           [have_wic=no])],
                             [have_wic=no])
        fi
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
    case "${host_os}" in
    emscripten*)
        if test x$ac_cv_header_emmintrin_h = xyes; then
            AC_MSG_CHECKING([whether the compiler supports -msimd128 and
                             <emmintrin.h>])
            saved_CFLAGS="$CFLAGS"
            simd_flag="-msimd128"
            CFLAGS="$CFLAGS $simd_flag"
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
                AM_CFLAGS="$AM_CFLAGS $simd_flag"
            fi
        fi
        ;;
    *)
    case "${host_cpu}" in
    x86_64|amd64)
        have_sse2=yes
        ;;

    i*86)
        if test x$ac_cv_header_emmintrin_h = xyes; then
            AC_MSG_CHECKING([whether the compiler supports SSE2 flags and
                             <emmintrin.h>])
            saved_CFLAGS="$CFLAGS"
            if test "x$using_cl" = xyes; then
                simd_flag="/arch:SSE2"
            else
                simd_flag="-msse2"
            fi
            CFLAGS="$CFLAGS $simd_flag"
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
                AM_CFLAGS="$AM_CFLAGS $simd_flag"
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

if test "x$enable_xsave_probe" = xno; then
    message_xsave_probe=disabled
elif test "x$have_xgetbv_intrin" = xyes; then
    message_xsave_probe=yes
else
    message_xsave_probe=no
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
                WICCODEC_LIBS="$WIC_LIBS -ladvapi32 -luser32 -loleaut32 \
                    -lgdi32"
            else
                WICCODEC_CFLAGS="$WIC_CFLAGS"
                WICCODEC_LIBS="$WIC_LIBS -loleaut32 -lgdi32"
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
                    ADDED_LIBS="-lole32 -lwindowscodecs -ladvapi32 -luser32 \
                        -loleaut32 -lgdi32"
                else
                    ADDED_CFLAGS="-I${prefix}/include"
                    ADDED_LIBS="-L${prefix}/lib -lole32 -lwindowscodecs \
                        -loleaut32 -lgdi32"
                fi
            else
                if test "x${using_cl}" = xyes; then
                    ADDED_CFLAGS=
                    ADDED_LIBS="-lole32 -lwindowscodecs -ladvapi32 -luser32 \
                        -loleaut32 -lgdi32"
                else
                    ADDED_CFLAGS="-I/usr/local/include"
                    ADDED_LIBS="-L/usr/local/lib -lole32 -lwindowscodecs \
                        -loleaut32 -lgdi32"
                fi
            fi
            CFLAGS="${CFLAGS} ${ADDED_CFLAGS}"
            LIBS="${LIBS} ${ADDED_LIBS}"
            AC_CHECK_HEADERS_PARALLEL([windows.h wincodec.h wincodecsdk.h],
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
    AM_PATH_PYTHON([3.0], [have_python=yes], [have_python=no])
    if test x$enable_python = xyes -a x$have_python = xno; then
        AC_MSG_ERROR([python is not available])
    fi
fi
if test x$enable_python != xno; then
    AC_MSG_CHECKING([for Python build module])
    if $PYTHON -c "import build" >/dev/null 2>&1; then
        AC_MSG_RESULT([yes])
    else
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([python build module was not found])
    fi
fi

ENABLE_PYTHON=0
if test x$enable_python != xno; then
    ENABLE_PYTHON=1
fi

AC_SUBST([ENABLE_PYTHON])

if test x$have_python = xyes; then
    message_python="$have_python: libsixel_wheel"
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

dnl Detect static analysis tools in configure phase so staticcheck can
dnl provide consistent skip messages when tools are unavailable.
AC_PATH_PROG([ACTIONLINT], [actionlint], [])
AC_PATH_PROG([SHELLCHECK], [shellcheck], [])
AC_PATH_PROG([CODESPELL], [codespell], [])
AC_SUBST([ACTIONLINT])
AC_SUBST([SHELLCHECK])
AC_SUBST([CODESPELL])

dnl Choose a fast POSIX shell for TAP tests and keep /bin/sh as fallback.
AC_ARG_VAR([SIXEL_TEST_SHELL],
           [Shell path used to run TAP test scripts (default: detect dash, sh, then fallback /bin/sh)])
AS_IF([test "x$SIXEL_TEST_SHELL" = x], [
  dnl Use separate cache variables for each lookup to avoid keeping
  dnl an earlier "no" result when probing the next candidate.
  AC_PATH_PROG([LS_DASH_PATH], [dash], [no])
  AS_IF([test "x$LS_DASH_PATH" != x -a "x$LS_DASH_PATH" != xno], [
    SIXEL_TEST_SHELL="$LS_DASH_PATH"
  ], [
    AC_PATH_PROG([LS_SH_PATH], [sh], [no])
    AS_IF([test "x$LS_SH_PATH" != x -a "x$LS_SH_PATH" != xno], [
      SIXEL_TEST_SHELL="$LS_SH_PATH"
    ], [
      SIXEL_TEST_SHELL="/bin/sh"
    ])
  ])
], [
  AC_MSG_NOTICE([SIXEL_TEST_SHELL preset to ${SIXEL_TEST_SHELL}])
])
AC_SUBST([SIXEL_TEST_SHELL])

#AC_DEFUN([LS_UPDATE_TIMESTAMP], [
#          touch -c aclocal.m4 Makefile.in configure config.h.in \
#                   converters/Makefile.in include/Makefile.in \
#                   python/Makefile.in src/Makefile.in tools/Makefile.in \
#                   assessment/Makefile.in wic/Makefile.in \
#                   tests/Makefile.in quicklook-extension/Makefile.in
#          ])
#LS_UPDATE_TIMESTAMP

AS_IF([test "x$enable_sanitizer" != "xno"], [
  dnl Apply sanitizer flags after configure probes complete.
  dnl This avoids polluting capability checks with sanitizer runtime
  dnl requirements that are unrelated to feature detection.
  CFLAGS="$CFLAGS $sixel_sanitizer_cflags"
  LDFLAGS="$LDFLAGS $sixel_sanitizer_ldflags"
])

AM_CONDITIONAL([HAVE_CURL], [test x$have_curl = xyes])
AM_CONDITIONAL([HAVE_WINHTTP], [test x$have_winhttp = xyes])
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
                 build-aux/lso-tap-driver.sh:build-aux/lso-tap-driver.sh.in
                 build-aux/resolve-test-tool-paths.sh:build-aux/resolve-test-tool-paths.sh.in
                 examples/Makefile
                 examples/drawing/Makefile
                 examples/opengl/Makefile
                 examples/python/Makefile
                 examples/ruby/Makefile
                 examples/windows/Makefile
                 images/Makefile
                 tests/Makefile
                 tests/data/inputs/thumbnailer/cases/0040/thumbnailers/valid-absolute.thumbnailer:tests/data/inputs/thumbnailer/cases/0040/thumbnailers/valid-absolute.thumbnailer.in
                 tests/data/inputs/thumbnailer/cases/0040/thumbnailers/missing-absolute.thumbnailer:tests/data/inputs/thumbnailer/cases/0040/thumbnailers/missing-absolute.thumbnailer.in
                 python/Makefile
                 ruby/Makefile
                 ruby/lib/libsixel/version_runtime.rb
                 gdk-pixbuf-loader/Makefile
                 quicklook-extension/Makefile
                 wic/Makefile
                ])

# Always generate the amalgamation Makefile so "make dist" can stage the
# directory even when the feature is disabled at configure time.
AC_CONFIG_FILES([amalgamation/Makefile])

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

AC_OUTPUT_PARALLEL

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
message_threads=$have_threads

echo ""
echo ""
echo "libsixel was configured as follows"
echo ""
echo "       Loader component        : $loaders"
echo "       Network access          : $net"
echo "       libwinpthread           : $have_winpthread"
echo "       internal threads        : $message_threads"
echo "       pkg-config dir          : $pkgconfigdir"
echo "       Bash completion dir     : $message_bashcompletiondir"
echo "       Zsh completion dir      : $message_zshcompletiondir"
echo "       QuickLook extension     : $message_quicklook_extension"
echo "       WIC codec               : $message_wiccodec"
echo "       Register WIC DLL        : $message_register_dll"
echo "       gdk-pixbuf SIXEL loader : $message_gdk_pixbuf_loader"
echo "       Thumbnailer command     : $message_thumbnailer"
echo "       python package          : $message_python"
echo "       ruby bindings           : $message_ruby"
echo "       simd                    : $message_simd"
echo "       xsave probe             : $message_xsave_probe"
echo "       gcov                    : $enable_gcov"
echo "       debugging               : $enable_debug"
echo "       tests                   : $enable_tests"
echo ""
echo ""
dnl :vim set et:
