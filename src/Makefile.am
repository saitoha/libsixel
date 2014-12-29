
if COND_GCOV
MAYBE_COVERAGE = --coverage
endif

CLEANFILES = *.gcno *.gcda *.gcov

lib_LTLIBRARIES = libsixel.la
libsixel_la_SOURCES = output.c fromsixel.c tosixel.c quant.c dither.c pixelformat.c scale.c
libsixel_la_CPPFLAGS = -I$(top_builddir)/include/
libsixel_la_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
libsixel_la_LDFLAGS = -version-info $(LS_LTVERSION)
libsixel_la_LIBADD = -lm

lib_LTLIBRARIES += libsixel-imageio.la
libsixel_imageio_la_SOURCES = loader.c frompnm.c frompnm.h stb_image.h
libsixel_imageio_la_CPPFLAGS = -I$(top_builddir)/include/
libsixel_imageio_la_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE) \
		$(GDK_PIXBUF_CFLAGS) $(LIBCURL_CFLAGS) \
		$(GD_CFLAGS) $(LIBJPEG_CFLAGS) $(LIBPNG_CFLAGS)
libsixel_imageio_la_LDFLAGS = -version-info $(LS_LTVERSION)
libsixel_imageio_la_LIBADD = $(GDK_PIXBUF_LIBS) $(LIBCURL_LIBS) $(GD_LIBS) \
		$(LIBJPEG_LIBS) $(LIBPNG_LIBS) libsixel.la

dist_man_MANS = sixel.5
