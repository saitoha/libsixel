
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
libwicsixel_la_CFLAGS = $(CFLAGS) \
                        $(AM_CFLAGS) \
                        $(WICCODEC_CFLAGS)

libwicsixel_la_LDFLAGS = -no-undefined -module -avoid-version
libwicsixel_la_LIBADD = $(WICCODEC_LIBS)

REGSVR32 = "$(shell cygpath $(SYSTEMROOT)/System32/regsvr32.exe)"

install-exec-hook:
	$(LIBTOOL) --mode=execute \
	  $(REGSVR32) -s \
	  "$(shell cygpath -w $(DESTDIR)$(pkglibdir)/libwicsixel.dll)"

uninstall-hook:
	$(LIBTOOL) --mode=execute \
	  $(REGSVR32) -u -s \
	  "$(shell cygpath -w $(DESTDIR)$(pkglibdir)/libwicsixel.dll)"
