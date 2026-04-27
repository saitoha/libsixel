BUILT_SOURCES =

CLEANFILES =

SIXEL_SHEBANG_FILE = @SIXEL_SHEBANG_FILE@
SIXEL_SHEBANG_HELPER = $(top_srcdir)/build-aux/sixel-apply-shebang.sh

AMALGAMATION_SIXEL = $(top_builddir)/amalgamation/sixel.c

if WANT_AMALGAMATED_TOOLS
BUILT_SOURCES += $(AMALGAMATION_SIXEL)
endif

if COND_GCOV
MAYBE_COVERAGE = --coverage
endif
if CFLAGS_HAVE_COVERAGE
MAYBE_COVERAGE = --coverage
endif

AMALGAMATION_LSQA_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include/ \
        -I$(top_srcdir)/include/ \
        -I$(top_srcdir) \
        $(LIBCURL_CFLAGS) \
        $(WINHTTP_CFLAGS) \
        $(LIBPNG_CFLAGS) \
        $(LIBTIFF_CFLAGS) \
        $(LIBJPEG_CFLAGS) \
        $(LCMS2_CFLAGS) \
        $(LIBWEBP_CFLAGS) \
        $(LIBRSVG_CFLAGS) \
        $(GDK_PIXBUF_CFLAGS) \
        $(GD_CFLAGS) \
        $(WIC_CFLAGS) \
        $(COREGRAPHICS_CFLAGS) \
        $(QUICKLOOK_CFLAGS) \
        $(CLIPBOARD_CFLAGS) \
        @THREAD_CFLAGS@
if AMALGAMATION_NEEDS_OBJC
AMALGAMATION_LSQA_CPPFLAGS += -x objective-c
endif

AMALGAMATION_LSQA_LIBS = -lm \
        $(LIBCURL_LIBS) \
        $(WINHTTP_LIBS) \
        $(LIBPNG_LIBS) \
        $(LIBTIFF_LIBS) \
        $(LIBJPEG_LIBS) \
        $(LCMS2_LIBS) \
        $(LIBWEBP_LIBS) \
        $(LIBRSVG_LIBS) \
        $(GDK_PIXBUF_LIBS) \
        $(GD_LIBS) \
        $(WIC_LIBS) \
        $(COREGRAPHICS_LIBS) \
        $(QUICKLOOK_LIBS) \
        $(CLIPBOARD_LIBS) \
        @THREAD_LIBS@

AMALGAMATION_DITHER_POLICY_SOURCES = \
        $(top_srcdir)/src/dither-policy-none.c \
        $(top_srcdir)/src/dither-policy-fs.c \
        $(top_srcdir)/src/dither-policy-atkinson.c \
        $(top_srcdir)/src/dither-policy-jajuni.c \
        $(top_srcdir)/src/dither-policy-stucki.c \
        $(top_srcdir)/src/dither-policy-burkes.c \
        $(top_srcdir)/src/dither-policy-sierra1.c \
        $(top_srcdir)/src/dither-policy-sierra2.c \
        $(top_srcdir)/src/dither-policy-sierra3.c \
        $(top_srcdir)/src/dither-policy-a-dither.c \
        $(top_srcdir)/src/dither-policy-x-dither.c \
        $(top_srcdir)/src/dither-policy-bluenoise.c \
        $(top_srcdir)/src/dither-policy-interframe.c \
        $(top_srcdir)/src/dither-policy-lso2.c

bin_PROGRAMS = lsqa

if WANT_AMALGAMATED_TOOLS
lsqa_SOURCES = $(AMALGAMATION_DITHER_POLICY_SOURCES)
nodist_lsqa_SOURCES = $(AMALGAMATION_SIXEL)
lsqa_CPPFLAGS = $(AMALGAMATION_LSQA_CPPFLAGS) \
        -I$(top_srcdir)/converters \
        -DSIXEL_AMALGAMATION_SPLIT_DITHER_POLICY=1 \
        -DBUILD_LSQA
lsqa_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE) \
        $(THREAD_CFLAGS)
lsqa_LDFLAGS = $(MAYBE_COVERAGE)
lsqa_LDADD = $(AMALGAMATION_LSQA_LIBS)
else
lsqa_SOURCES = $(srcdir)/lsqa.c \
               $(srcdir)/assessment.c \
               $(srcdir)/assessment.h \
               $(top_srcdir)/converters/cli.c
lsqa_CPPFLAGS = -I$(top_builddir)/include/ \
                -I$(top_srcdir)/src \
                -I$(top_srcdir)/assessment \
                -I$(top_srcdir)/converters \
                $(AM_CPPFLAGS)
lsqa_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE) \
        $(THREAD_CFLAGS)
lsqa_LDFLAGS = $(MAYBE_COVERAGE)
lsqa_LDADD = $(top_builddir)/src/libsixel.la
lsqa_LDADD += -lm \
        $(LIBCURL_LIBS) \
        $(WINHTTP_LIBS) \
        $(LIBPNG_LIBS) \
        $(LIBTIFF_LIBS) \
        $(LIBJPEG_LIBS) \
        $(LCMS2_LIBS) \
        $(LIBWEBP_LIBS) \
        $(GDK_PIXBUF_LIBS) \
        $(GD_LIBS) \
        $(WIC_LIBS) \
        $(COREGRAPHICS_LIBS) \
        $(QUICKLOOK_LIBS) \
        $(CLIPBOARD_LIBS) \
        @THREAD_LIBS@
if BUILDING_ON_WINDOWS
if USING_CL
EXTRA_lsqa_DEPENDENCIES = $(top_builddir)/src/.libs/sixel.dll.lib
endif
endif
endif

if WITH_SHEBANG_FILE
SIXEL_SHEBANG_TARGETS = $(bin_PROGRAMS)
SIXEL_SHEBANG_STAMP = assessment-shebang.stamp

$(SIXEL_SHEBANG_STAMP): $(SIXEL_SHEBANG_TARGETS)
	@if test -n "$(SIXEL_SHEBANG_TARGETS)"; then \
		$(SIXEL_SHEBANG_HELPER) "$(SIXEL_SHEBANG_FILE)" \
			$(SIXEL_SHEBANG_TARGETS); \
	fi
	@touch $@

all-local: $(SIXEL_SHEBANG_STAMP)
CLEANFILES += $(SIXEL_SHEBANG_STAMP)
endif

.PHONY: prelink-objs

# Build only lsqa object files before the final executable link.
#
# The top-level Makefile uses this split target to run object compilation in
# parallel with libsixel shared library creation.
prelink-objs: $(BUILT_SOURCES) $(lsqa_OBJECTS)
