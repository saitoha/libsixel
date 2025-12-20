if COND_GCOV
MAYBE_COVERAGE = --coverage
endif

EXTRA_DIST = meson.build

BUILT_SOURCES = $(builddir)/sixel.c
CLEANFILES = $(builddir)/sixel.c

AMALGAMATION_GEN = $(top_srcdir)/tools/gen-amalgamation.sh
AMALGAMATION_SRC_DEPS = $(wildcard $(top_srcdir)/src/*.[chm])
AMALGAMATION_CONVERTER_DEPS = $(wildcard $(top_srcdir)/converters/*.[ch])
AMALGAMATION_ASSESSMENT_DEPS = $(wildcard $(top_srcdir)/assessment/*.[ch])
AMALGAMATION_PUBLIC_DEPS = $(wildcard $(top_srcdir)/include/*.h \
        $(top_srcdir)/include/*.h.in)
AMALGAMATION_BUILD_DEPS = $(wildcard $(top_builddir)/config.h \
        $(top_builddir)/include/*.h \
        $(top_builddir)/converters/completion_embed.h)
AMALGAMATION_DEPS = $(AMALGAMATION_GEN) $(AMALGAMATION_SRC_DEPS) \
        $(AMALGAMATION_CONVERTER_DEPS) $(AMALGAMATION_ASSESSMENT_DEPS) \
        $(AMALGAMATION_PUBLIC_DEPS) $(AMALGAMATION_BUILD_DEPS)

# Keep this recipe tab-indented; GNU make requires tabs for commands.
$(builddir)/sixel.c: $(AMALGAMATION_DEPS)
	$(AM_V_GEN)$(SHELL) $(AMALGAMATION_GEN) "$@" \
	                "$(top_srcdir)" "$(top_builddir)"

# Keep this recipe tab-indented; GNU make requires tabs for commands.
all-local: $(builddir)/sixel.c
