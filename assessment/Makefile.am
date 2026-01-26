BUILT_SOURCES =

CLEANFILES =

SIXEL_SHEBANG_FILE = @SIXEL_SHEBANG_FILE@
SIXEL_SHEBANG_HELPER = $(top_srcdir)/build-aux/sixel-apply-shebang.sh

AMALGAMATION_SIXEL = $(top_builddir)/amalgamation/sixel.c

if WANT_AMALGAMATED_TOOLS
BUILT_SOURCES += $(AMALGAMATION_SIXEL)

# Keep this recipe tab-indented; GNU make requires tabs for commands.
$(AMALGAMATION_SIXEL): $(top_builddir)/amalgamation/Makefile
	$(AM_V_GEN)$(MAKE) -C $(top_builddir)/converters completion_embed.h
	$(AM_V_GEN)$(MAKE) -C $(top_builddir)/amalgamation sixel.c
endif

if COND_GCOV
MAYBE_COVERAGE = --coverage
endif
if CFLAGS_HAVE_COVERAGE
MAYBE_COVERAGE = --coverage
endif

AMALGAMATION_LSQA_CPPFLAGS = $(AM_CPPFLAGS) \
        $(LIBCURL_CFLAGS) \
        $(WINHTTP_CFLAGS) \
        $(LIBPNG_CFLAGS) \
        $(LIBJPEG_CFLAGS) \
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
        $(LIBJPEG_LIBS) \
        $(GDK_PIXBUF_LIBS) \
        $(GD_LIBS) \
        $(WIC_LIBS) \
        $(COREGRAPHICS_LIBS) \
        $(QUICKLOOK_LIBS) \
        $(CLIPBOARD_LIBS) \
        @THREAD_LIBS@

bin_PROGRAMS = lsqa

if WANT_AMALGAMATED_TOOLS
lsqa_SOURCES =
nodist_lsqa_SOURCES = $(AMALGAMATION_SIXEL)
lsqa_CPPFLAGS = $(AMALGAMATION_LSQA_CPPFLAGS) -DBUILD_LSQA
lsqa_CFLAGS = $(CFLAGS) $(AM_CFLAGS) $(MAYBE_COVERAGE)
lsqa_LDFLAGS = $(MAYBE_COVERAGE)
lsqa_LDADD = $(AMALGAMATION_LSQA_LIBS)
else
lsqa_SOURCES = $(srcdir)/lsqa.c
lsqa_CPPFLAGS = -I$(top_builddir)/include/ \
                -I$(top_srcdir)/src \
                $(AM_CPPFLAGS)
lsqa_CFLAGS = $(AM_CFLAGS) $(MAYBE_COVERAGE)
lsqa_LDFLAGS = $(MAYBE_COVERAGE)
lsqa_LDADD = $(top_builddir)/src/libsixel.la
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
