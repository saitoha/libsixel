DIST_SUBDIRS = $(SUBDIRS)

srcdir = @srcdir@
top_builddir = @top_builddir@
builddir = @builddir@
VPATH = @srcdir@

BUILT_SOURCES = completion_embed.h

BASH = @BASH@

bin_PROGRAMS =
dist_man_MANS =

bashcompletiondir = @bashcompletiondir@
dist_bashcompletion_DATA =

zshcompletiondir = @zshcompletiondir@
dist_zshcompletion_DATA =

CONVERTER_TEST_LIST =
if WANT_IMG2SIXEL
if HAVE_BASH
CONVERTER_TEST_LIST += \
	$(top_srcdir)/tests/converters/test_0001_invalid_option.bash \
	$(top_srcdir)/tests/converters/test_0002_stdin.bash \
	$(top_srcdir)/tests/converters/test_0003_information.bash \
	$(top_srcdir)/tests/converters/test_0004_conversion_options.bash \
	$(top_srcdir)/tests/converters/test_0005_dcs_arguments.bash \
	$(top_srcdir)/tests/converters/test_0006_dcs_variations.bash \
	$(top_srcdir)/tests/converters/test_0007_animation.bash \
	$(top_srcdir)/tests/converters/test_0008_progressive_jpeg.bash
if HAVE_PNG
CONVERTER_TEST_LIST += $(top_srcdir)/tests/converters/test_0009_pngsuite.bash
endif
if HAVE_CURL
CONVERTER_TEST_LIST += $(top_srcdir)/tests/converters/test_0010_curl.bash
endif
if WANT_SIXEL2PNG
CONVERTER_TEST_LIST += $(top_srcdir)/tests/converters/test_0011_basic.bash
endif
CONVERTER_TEST_LIST += \
        $(top_srcdir)/tests/converters/test_0012_regressions.bash \
        $(top_srcdir)/tests/converters/test_0013_documentation.bash \
        $(top_srcdir)/tests/converters/test_0014_option_matching.bash \
        $(top_srcdir)/tests/converters/test_0015_argument_shift.bash \
        $(top_srcdir)/tests/converters/test_0016_assessment_spool.bash \
        $(top_srcdir)/tests/converters/test_0017_clipboard.bash
endif
endif


if COND_GCOV
MAYBE_COVERAGE = --coverage
endif

CLEANFILES = *.gcno *.gcda *.gcov \
  $(builddir)/tmp/*.png \
  $(builddir)/tmp/*.sixel \
  $(builddir)/tmp/*.txt \
  $(builddir)/tmp/*.pipe \
  $(builddir)/tmp/server.py \
  $(builddir)/tmp/server.key \
  $(builddir)/tmp/server.crt \
  $(builddir)/tmp/testfile \
  completion_embed.h

# +--------------------------------------------------------------+
# | Embedded completion header generation pipeline (ASCII view). |
# +--------------------------------------------------------------+
# bash/img2sixel ----\
#                      >--[gen_completion_embed.awk]--> completion_embed.h
# zsh/_img2sixel  ----/
completion_embed.h: \
	$(srcdir)/shell-completion/bash/img2sixel \
	$(srcdir)/shell-completion/zsh/_img2sixel \
	$(top_srcdir)/tools/gen_completion_embed.awk
	$(AM_V_GEN)$(AWK) -f $(top_srcdir)/tools/gen_completion_embed.awk \
	       $(srcdir)/shell-completion/bash/img2sixel \
	       $(srcdir)/shell-completion/zsh/_img2sixel > $@

if WANT_IMG2SIXEL
bin_PROGRAMS += img2sixel
img2sixel_SOURCES = $(srcdir)/img2sixel.c \
                    $(srcdir)/completion_utils.c \
                    $(srcdir)/completion_utils.h \
                    $(srcdir)/malloc_stub.c \
                    $(srcdir)/malloc_stub.h \
                    $(srcdir)/getopt_stub.h
# Track the generated header so dependency ordering stays stable.
nodist_img2sixel_SOURCES = completion_embed.h
# Ensure older toolchains notice the generated completion header.
img2sixel_CPPFLAGS = -DHAVE_COMPLETION_EMBED_H \
        -I$(top_builddir)/include/ \
        $(AM_CPPFLAGS)
img2sixel_CFLAGS = $(AM_CFLAGS) $(MAYBE_COVERAGE)
img2sixel_LDFLAGS = $(MAYBE_COVERAGE)
img2sixel_LDADD = $(top_builddir)/src/libsixel.la
dist_man_MANS += $(srcdir)/img2sixel.1
dist_bashcompletion_DATA += $(srcdir)/shell-completion/bash/img2sixel
dist_zshcompletion_DATA += $(srcdir)/shell-completion/zsh/_img2sixel
endif

if WANT_SIXEL2PNG
bin_PROGRAMS += sixel2png
sixel2png_SOURCES = $(srcdir)/sixel2png.c \
                    $(srcdir)/malloc_stub.c \
                    $(srcdir)/malloc_stub.h \
                    $(srcdir)/getopt_stub.h
sixel2png_CPPFLAGS = -I$(top_builddir)/include/ $(AM_CPPFLAGS)
sixel2png_CFLAGS = $(AM_CFLAGS) $(MAYBE_COVERAGE)
sixel2png_LDFLAGS = $(MAYBE_COVERAGE)
sixel2png_LDADD = $(top_builddir)/src/libsixel.la
dist_man_MANS += $(srcdir)/sixel2png.1
endif

check: test_impl

test: all test_impl

test_impl:
	@if test "x$(BASH)" = x -o "x$(BASH)" = xno; then \
		echo '[skipped] bash is not available; converter tests skipped'; \
		exit 0; \
	fi
	@if test -z "$(CONVERTER_TEST_LIST)"; then \
		echo '[skipped] no converter tests are enabled'; \
		exit 0; \
	fi
	@echo '[start]'
	@set -e; \
	for test_script in $(CONVERTER_TEST_LIST); do \
		echo "[$$(basename $$test_script)]"; \
		TOP_SRCDIR=$(top_srcdir) \
		SRC_DIR=$(srcdir) \
		BUILD_DIR=$(builddir) \
		TMP_DIR=$(builddir)/tmp \
		WINE="$(WINE)" \
		WINEEXT="$(WINEEXT)" \
		$(BASH) "$$test_script" || exit $$?; \
	done

	@echo '[succeeded]'
winetest: all
	WINE=wine $(MAKE) test
