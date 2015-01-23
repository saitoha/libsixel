#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([demo], [1.0.0], [user@zuse.jp])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([LIBSIXEL], [libsixel], [have_libsixel=yes], [have_libsixel=no])
if test x$have_libsixel != xno; then
    AC_MSG_NOTICE([libsixel: $have_libsixel])
    AC_DEFINE(HAVE_LIBSIXEL, 1, [whether libsixel is available])
fi
AC_SUBST(LIBSIXEL_CFLAGS)
AC_SUBST(LIBSIXEL_LIBS)

PKG_CHECK_MODULES([OSMESA], [osmesa], [have_osmesa=yes], [have_osmesa=no])
if test x$have_osmesa != xno; then
    AC_MSG_NOTICE([osmesa: $have_osmesa])
    AC_DEFINE(HAVE_OSMESA, 1, [whether osmesa is available])
fi
AC_SUBST(OSMESA_CFLAGS)
AC_SUBST(OSMESA_LIBS)

# Checks for header files.
#AC_PATH_X
AC_CHECK_HEADERS([memory.h stdlib.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strstr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
