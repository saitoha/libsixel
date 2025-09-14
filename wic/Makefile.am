
srcdir = @srcdir@
top_srcdir = @top_srcdir@
VPATH = @srcdir@
prefix = @prefix@
exec_prefix = @exec_prefix@
top_builddir = @top_builddir@
builddir = @builddir@

CLEANFILES = *.o *.obj

pkglib_LTLIBRARIES = libwicsixel.la
libwicsixel_la_SOURCES = $(srcdir)/wic.c \
                         $(top_builddir)/src/fromsixel.c \
                         $(top_builddir)/src/allocator.c \
                         $(top_builddir)/src/status.c

libwicsixel_la_CPPFLAGS = -I$(top_builddir)/include/
libwicsixel_la_CFLAGS = $(AM_CFLAGS)
libwicsixel_la_LDFLAGS = -no-undefined -module -avoid-version
libwicsixel_la_LIBADD = -lole32 -lwindowscodecs


