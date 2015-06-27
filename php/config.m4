dnl
dnl $ Id: $
dnl

PHP_ARG_ENABLE(sixel, whether to enable sixel functions,
[  --enable-sixel         Enable sixel support])

if test "$PHP_SIXEL" != "no"; then

PHP_ARG_WITH(libsixel, whether libsixel is available,[  --with-libsixel[=DIR] With libsixel support])


  
  if test -z "$PKG_CONFIG"
  then
	AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi
  if test "$PKG_CONFIG" = "no"
  then
	AC_MSG_ERROR([required utility 'pkg-config' not found])
  fi

  if ! $PKG_CONFIG --exists libsixel
  then
	AC_MSG_ERROR(['libsixel' not known to pkg-config])
  fi

  if ! $PKG_CONFIG --atleast-version 1.5 libsixel
  then
	PKG_VERSION=`$PKG_CONFIG --modversion libsixel`
	AC_MSG_ERROR(['libsixel'\ is version $PKG_VERSION, 1.5 required])
  fi

  PHP_EVAL_INCLINE(`$PKG_CONFIG --cflags-only-I libsixel`)
  PHP_EVAL_LIBLINE(`$PKG_CONFIG --libs libsixel`, SIXEL_SHARED_LIBADD)

  export OLD_CPPFLAGS="$CPPFLAGS"
  export CPPFLAGS="$CPPFLAGS $INCLUDES -DHAVE_LIBSIXEL"
  AC_CHECK_HEADER([sixel.h], [], AC_MSG_ERROR('sixel.h' header not found))
  export CPPFLAGS="$OLD_CPPFLAGS"

  export OLD_CPPFLAGS="$CPPFLAGS"
  export CPPFLAGS="$CPPFLAGS $INCLUDES -DHAVE_SIXEL"

  AC_MSG_CHECKING(PHP version)
  AC_TRY_COMPILE([#include <php_version.h>], [
#if PHP_VERSION_ID < 50000
#error  this extension requires at least PHP version 5.0.0
#endif
],
[AC_MSG_RESULT(ok)],
[AC_MSG_ERROR([need at least PHP 5.0.0])])

  export CPPFLAGS="$OLD_CPPFLAGS"


  PHP_SUBST(SIXEL_SHARED_LIBADD)
  AC_DEFINE(HAVE_SIXEL, 1, [ ])

  PHP_NEW_EXTENSION(sixel, sixel.c , $ext_shared)

fi

