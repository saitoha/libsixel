
if COND_GCOV
MAYBE_COVERAGE = --coverage
endif

CLEANFILES = *.gcno *.gcda *.gcov *.log

lib_LTLIBRARIES = libsixel.la
libsixel_la_SOURCES = output.c \
		output.h \
		fromsixel.c \
		tosixel.c \
		quant.c \
		quant.h \
		dither.c \
		dither.h \
		frame.c \
		frame.h \
		pixelformat.c \
		pixelformat.h \
		scale.c \
		loader.c \
		frompnm.c \
		frompnm.h \
                fromgif.c \
                fromgif.h \
		encoder.c \
		encoder.h \
		decoder.c \
		decoder.h \
		stb_image.h \
		writer.c \
		stb_image_write.c \
		stb_image_write.h
libsixel_la_CPPFLAGS = -I$(top_builddir)/include/
libsixel_la_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE) \
		$(LIBCURL_CFLAGS) \
		$(LIBPNG_CFLAGS) \
		$(LIBJPEG_CFLAGS) \
		$(GDK_PIXBUF_CFLAGS) \
		$(GD_CFLAGS)
libsixel_la_LDFLAGS = -version-info $(LS_LTVERSION)
libsixel_la_LIBADD = -lm \
		$(LIBCURL_LIBS) \
		$(LIBPNG_LIBS) \
		$(LIBJPEG_LIBS) \
		$(GDK_PIXBUF_LIBS) \
		$(GD_LIBS)

if COND_TESTS
noinst_PROGRAMS = tests
tests_SOURCES = tests.c
tests_CPPFLAGS = -I$(top_builddir)/include/ \
		 -I$(top_builddir)/src/ $(AM_CPPFLAGS)
tests_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
tests_LDADD = $(top_builddir)/src/libsixel.la
endif

unittest: all
if COND_TESTS
	./tests
endif

dist_man_MANS = sixel.5
