
if COND_GCOV
MAYBE_COVERAGE = --coverage
endif

CLEANFILES = *.gcno *.gcda *.gcov *.log test-output.* tests

lib_LTLIBRARIES = libsixel.la
libsixel_la_SOURCES = \
		output.c \
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
		chunk.c \
		chunk.h \
		loader.c \
		loader.h \
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
		writer.h \
		stb_image_write.c \
		stb_image_write.h \
		status.c \
		status.h \
		malloc_stub.c \
		malloc_stub.h \
		allocator.c \
		allocator.h \
		tty.c \
		tty.h \
		rgblookup.h
libsixel_la_CPPFLAGS = -I$(top_builddir)/include/
libsixel_la_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE) \
		$(LIBCURL_CFLAGS) \
		$(LIBPNG_CFLAGS) \
		$(LIBJPEG_CFLAGS) \
		$(GDK_PIXBUF_CFLAGS) \
		$(GD_CFLAGS)
libsixel_la_LDFLAGS = -no-undefined -version-info $(LS_LTVERSION)
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
tests_LDADD = libsixel.la
endif

dist_man_MANS = sixel.5
EXTRA_DIST = rgblookup.gperf

unittest: all
if COND_TESTS
	./tests
endif

gen-rgblookup: rgblookup.gperf
	gperf -C -N lookup_rgb --ignore-case $< |\
	sed 's/{""}/\{"", 0, 0, 0\}/g'          |\
	astyle                                  > rgblookup.h


