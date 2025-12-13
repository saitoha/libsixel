if COND_GCOV
MAYBE_COVERAGE = --coverage
endif

EXTRA_DIST = meson.build

BUILT_SOURCES = $(builddir)/sixel.c
CLEANFILES = $(builddir)/sixel.c

AMALGAMATION_GEN = $(top_srcdir)/tools/gen-amalgamation.sh
AMALGAMATION_SRC_DEPS = $(wildcard $(top_srcdir)/src/*.[chm])
AMALGAMATION_PUBLIC_DEPS = $(wildcard $(top_srcdir)/include/*.h \
        $(top_srcdir)/include/*.h.in)
AMALGAMATION_BUILD_DEPS = $(wildcard $(top_builddir)/config.h \
        $(top_builddir)/include/*.h)
AMALGAMATION_DEPS = $(AMALGAMATION_GEN) $(AMALGAMATION_SRC_DEPS) \
        $(AMALGAMATION_PUBLIC_DEPS) $(AMALGAMATION_BUILD_DEPS)

$(builddir)/sixel.c: $(AMALGAMATION_DEPS)
	$(AM_V_GEN)$(SHELL) $(AMALGAMATION_GEN) "$@" \
	    "$(top_srcdir)" "$(top_builddir)"

noinst_LTLIBRARIES = libsixel-amalgamation.la
libsixel_amalgamation_la_SOURCES = $(builddir)/sixel.c
libsixel_amalgamation_la_CPPFLAGS = -I$(top_builddir) \
	-I$(top_srcdir) -I$(top_srcdir)/src -I$(top_builddir)/include/ \
	$(AM_CPPFLAGS) @THREAD_CPPFLAGS@
libsixel_amalgamation_la_CFLAGS = $(AM_CFLAGS) $(MAYBE_COVERAGE) \
	$(LIBCURL_CFLAGS) $(WINHTTP_CFLAGS) $(LIBPNG_CFLAGS) \
	$(LIBJPEG_CFLAGS) $(GDK_PIXBUF_CFLAGS) $(GD_CFLAGS) \
	$(WIC_CFLAGS) $(COREGRAPHICS_CFLAGS) $(QUICKLOOK_CFLAGS) \
	$(CLIPBOARD_CFLAGS) $(ONNXRUNTIME_CFLAGS) @THREAD_CFLAGS@
libsixel_amalgamation_la_LDFLAGS = $(MAYBE_COVERAGE)
if BUILDING_ON_WINDOWS
libsixel_amalgamation_la_LDFLAGS += -no-undefined
endif
libsixel_amalgamation_la_LIBADD = -lm $(LIBCURL_LIBS) $(WINHTTP_LIBS) \
	$(LIBPNG_LIBS) $(LIBJPEG_LIBS) $(GDK_PIXBUF_LIBS) $(GD_LIBS) \
	$(WIC_LIBS) $(COREGRAPHICS_LIBS) $(QUICKLOOK_LIBS) \
	$(CLIPBOARD_LIBS) $(ONNXRUNTIME_LIBS) @THREAD_LIBS@

all-local: $(builddir)/sixel.c
