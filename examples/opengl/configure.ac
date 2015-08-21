#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([demo], [1.0.0], [user@zuse.jp])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([LIBSIXEL], [libsixel], [have_libsixel=yes], [have_libsixel=no])
if test x$have_libsixel != xno; then
    AC_MSG_NOTICE([libsixel: $have_libsixel])
    AC_DEFINE(HAVE_LIBSIXEL, 1, [whether libsixel is available])
else
   AC_MSG_ERROR(libsixel is not available.)
fi
AC_SUBST(LIBSIXEL_CFLAGS)
AC_SUBST(LIBSIXEL_LIBS)

if test x$have_gl != xyes; then
    PKG_CHECK_MODULES([GL], [osmesa], [have_osmesa=yes], [have_osmesa=no])
    if test x$have_osmesa != xno; then
        have_gl=yes
        AC_MSG_NOTICE([osmesa: $have_osmesa])
        AC_DEFINE(HAVE_OSMESA, 1, [whether osmesa is available])
    fi
fi

if test x$have_gl != xyes; then
    case $host in
        *-apple-darwin*)
            have_gl=yes
            GL_CFLAGS=-Wno-deprecated-declarations
            GL_LIBS="-framework OpenGL"
            AC_DEFINE(HAVE_CGL, 1, [whether CGL is available])
            ;;
    esac
fi

if test x$have_gl != xyes; then
    AC_PATH_XTRA
    if test x$X_DISPLAY_MISSING = x; then
        AC_CHECK_HEADER([GL/gl.h], [
            AC_CHECK_HEADER([GL/glx.h], [
                AC_CHECK_LIB([GL], [glXCreateContext], [
                    have_gl=yes
                    GL_CFLAGS=
                    GL_LIBS=-lGL
                    AC_DEFINE(HAVE_GLX, 1, [whether GLX is available])
                ])
            ])
        ])
    fi
    AC_SUBST(X_CFLAGS)
    AC_SUBST(X_LIBS)
fi

if test x$have_gl != xyes; then
   AC_MSG_ERROR([Please install osmesa (e.g. "apt-get install libosmesa6-dev")])
fi

AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)

# Checks for header files.
AC_CHECK_HEADERS([sys/select.h termios.h fcntl.h sys/ioctl.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strstr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
