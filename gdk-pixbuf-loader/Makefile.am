plugindir = $(GDK_PIXBUF_MODULEDIR)
plugin_LTLIBRARIES = libpixbufloader-sixel.la

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/include -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS) -DGDK_PIXBUF_ENABLE_BACKEND
AM_CFLAGS = $(CFLAGS)

libpixbufloader_sixel_la_SOURCES = sixel-pixbuf-loader.c
libpixbufloader_sixel_la_LDFLAGS = -module -avoid-version -shared
libpixbufloader_sixel_la_LIBADD = $(GDK_PIXBUF_LIBS) \
        $(top_builddir)/src/libsixel.la

EXTRA_DIST = README.md
