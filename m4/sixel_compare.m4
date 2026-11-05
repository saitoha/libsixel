# SPDX-License-Identifier: MIT
AC_DEFUN([SIXEL_COMPARE_SETUP], [
  AS_IF([! command -v cmp >/dev/null 2>&1 &&
         ! command -v od >/dev/null 2>&1], [
    AC_MSG_ERROR([exact file comparison requires cmp or the baseline od utility])
  ])
  # Keep native system tools first. Configure/Libtool can use the source-only
  # fallback before make builds the faster, bounded-memory comparator.
  sixel_compare_root=`cd "$srcdir" && pwd`
  PATH="$PATH:$sixel_compare_root/build-aux/compare"
  export PATH
])

AC_DEFUN([SIXEL_COMPARE_BUILD], [
  AC_ARG_VAR([CC_FOR_BUILD], [C compiler for build-machine check tools])
  AC_ARG_VAR([CFLAGS_FOR_BUILD], [C flags for build-machine check tools])
  AS_IF([test -z "$CC_FOR_BUILD"], [
    AC_CHECK_PROGS([CC_FOR_BUILD], [cc gcc clang], [])
    # A compiler such as emcc may run its configure probes through a runtime
    # yet still not produce a build-machine utility. Prefer a host compiler.
    AS_IF([test -z "$CC_FOR_BUILD" && test "$cross_compiling" = no],
          [CC_FOR_BUILD=$CC])
  ])
  AC_MSG_CHECKING([whether a native file comparator can run])
  sixel_have_compare=no
  # A relative-only probe misses native Windows CRTs under Cygwin shells.
  sixel_compare_source=`cd "$srcdir" && pwd`/tools/compare.c
  SIXEL_COMPARE_EXEEXT=
  # MSVC leaves an object beside the build invocation. Use a conftest source
  # name so the capability probe owns every compiler/linker byproduct.
  AS_IF([test -n "$CC_FOR_BUILD" &&
      cat "$srcdir/tools/compare.c" > conftest-compare.c &&
      $CC_FOR_BUILD $CFLAGS_FOR_BUILD -o conftest-compare \
        conftest-compare.c >&AS_MESSAGE_LOG_FD 2>&1], [
    test ! -f conftest-compare.exe || SIXEL_COMPARE_EXEEXT=.exe
    AS_IF([./conftest-compare$SIXEL_COMPARE_EXEEXT -s \
        "$sixel_compare_source" conftest-compare.c &&
        printf '\n' >> conftest-compare.c &&
        { ./conftest-compare$SIXEL_COMPARE_EXEEXT -s \
            "$sixel_compare_source" conftest-compare.c; \
          test "$?" = 1; } &&
        { ./conftest-compare$SIXEL_COMPARE_EXEEXT -s \
            "$srcdir/tools" "$srcdir/tools/compare.c"; \
          test "$?" = 2; } &&
        { ./conftest-compare$SIXEL_COMPARE_EXEEXT -s \
            "$sixel_compare_source" conftest-compare.c/missing; \
          test "$?" = 2; }], [sixel_have_compare=yes])
  ])
  rm -f conftest-compare conftest-compare.exe conftest-compare.c \
    conftest-compare.obj conftest-compare.pdb conftest-compare.ilk \
    conftest-compare.exe.manifest
  AC_MSG_RESULT([$sixel_have_compare])
  AC_SUBST([SIXEL_COMPARE_EXEEXT])
  AC_SUBST([sixel_have_compare])
  AM_CONDITIONAL([BUILD_TEST_COMPARE], [test "$sixel_have_compare" = yes])
])
