if COND_GCOV
MAYBE_COVERAGE = --coverage
endif
if CFLAGS_HAVE_COVERAGE
MAYBE_COVERAGE = --coverage
endif

EXTRA_DIST = meson.build

BUILT_SOURCES = $(builddir)/sixel.c
CLEANFILES = $(builddir)/sixel.c

AMALGAMATION_GEN = $(top_srcdir)/tools/gen-amalgamation.sh
AMALGAMATION_SRC_DEPS = $(wildcard $(top_srcdir)/src/*.[chm])
AMALGAMATION_CONVERTER_DEPS = $(wildcard \
        $(top_srcdir)/converters/*.[chm])
AMALGAMATION_ASSESSMENT_DEPS = $(wildcard \
        $(top_srcdir)/assessment/*.[chm])
AMALGAMATION_GDK_PIXBUF_DEPS = $(wildcard \
        $(top_srcdir)/gdk-pixbuf-loader/*.[chm])
# The generator embeds registered tests recursively.  Keep these levels in
# sync with the deepest tracked test source so incremental builds cannot reuse
# an amalgamation generated before a test-local identifier rename.
AMALGAMATION_TEST_DEPS = $(wildcard \
        $(top_srcdir)/tests/*.[chm] \
        $(top_srcdir)/tests/*/*.[chm] \
        $(top_srcdir)/tests/*/*/*.[chm] \
        $(top_srcdir)/tests/*/*/*/*.[chm])
AMALGAMATION_PUBLIC_DEPS = $(wildcard $(top_srcdir)/include/*.h \
        $(top_srcdir)/include/*.h.in)
AMALGAMATION_BUILD_DEPS = $(wildcard $(top_builddir)/config.h \
        $(top_builddir)/include/*.h \
        $(top_builddir)/converters/completion_embed.h)
AMALGAMATION_DEPS = $(AMALGAMATION_GEN) $(AMALGAMATION_SRC_DEPS) \
        $(AMALGAMATION_CONVERTER_DEPS) $(AMALGAMATION_ASSESSMENT_DEPS) \
        $(AMALGAMATION_GDK_PIXBUF_DEPS) $(AMALGAMATION_TEST_DEPS) \
        $(AMALGAMATION_PUBLIC_DEPS) \
        $(AMALGAMATION_BUILD_DEPS) Makefile

# Keep this recipe tab-indented; GNU make requires tabs for commands.
$(builddir)/sixel.c: $(AMALGAMATION_DEPS)
	$(AM_V_GEN)$(SHELL) $(AMALGAMATION_GEN) "$@" \
	                "$(top_srcdir)" "$(top_builddir)"

# Keep this recipe tab-indented; GNU make requires tabs for commands.
all-local: $(builddir)/sixel.c
