DIST_SUBDIRS = $(SUBDIRS)

srcdir = @srcdir@
top_builddir = @top_builddir@
builddir = @builddir@
VPATH = @srcdir@

BASH = @BASH@

bin_PROGRAMS =
dist_man_MANS =

bashcompletiondir = @bashcompletiondir@
dist_bashcompletion_DATA =

zshcompletiondir = @zshcompletiondir@
dist_zshcompletion_DATA =

CONVERTER_TEST_NAMES =
if WANT_IMG2SIXEL
if HAVE_BASH
CONVERTER_TEST_NAMES += \
        0001_invalid_option.t \
        0002_stdin.t \
        0003_information.t \
        0004_conversion_options.t \
        0005_dcs_arguments.t \
        0006_dcs_variations.t \
        0007_animation.t \
        0008_progressive_jpeg.t
if HAVE_PNG
CONVERTER_TEST_NAMES += 0009_pngsuite.t
endif
if HAVE_CURL
CONVERTER_TEST_NAMES += 0010_curl.t
endif
if WANT_SIXEL2PNG
CONVERTER_TEST_NAMES += 0011_basic.t
endif
CONVERTER_TEST_NAMES += \
        0012_regressions.t \
        0013_documentation.t
endif
endif

if WANT_IMG2SIXEL
if HAVE_BASH
# ----------------------------------------------------------------------
#  Automake relationships
#
#      converters/Makefile.am (this file)
#                  |
#                  v
#      tests/t/*.t  <-- real scripts live here
#                  ^
#                  |
#          +--------------------------+
#          | check-converter-tests    |
#          +------------+-------------+
#                       |
#         +-------------v-------------+
#         | env + TAP driver + bash   |
#         +---------------------------+
#
#  We keep only the basename list in CONVERTER_TEST_NAMES and derive
#  multiple path variants from it.  Automake now stays out of the loop,
#  so the manual harness below is responsible for feeding scripts to the
#  shared TAP driver and compiler pair.
# ----------------------------------------------------------------------
CONVERTER_TEST_SRCDIR = $(top_srcdir)/tests/t
CONVERTER_TEST_SCRIPT_PATHS = $(addprefix $(CONVERTER_TEST_SRCDIR)/, $(CONVERTER_TEST_NAMES))
CONVERTER_TAP_DRIVER = $(SHELL) $(top_srcdir)/build-aux/tap-driver.sh
CONVERTER_TAP_COMPILER = $(BASH)
CONVERTER_TEST_ENV_VARS = \
        TOP_SRCDIR="$(top_srcdir)" \
        SRC_DIR="$(srcdir)" \
        BUILD_DIR="$(builddir)" \
        TMP_DIR="$(builddir)/tmp" \
        WINE="$(WINE)" \
        WINEEXT="$(WINEEXT)"
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
  $(builddir)/tmp/logs/*.log

if WANT_IMG2SIXEL
bin_PROGRAMS += img2sixel
img2sixel_SOURCES = $(srcdir)/img2sixel.c \
                    $(srcdir)/malloc_stub.c \
                    $(srcdir)/malloc_stub.h \
                    $(srcdir)/getopt_stub.h
img2sixel_CPPFLAGS = -I$(top_builddir)/include/ $(AM_CPPFLAGS)
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

winetest: all
        WINE=wine $(MAKE) check

check-am: check-converter-tests

check-converter-tests:
	@if test "x$(BASH)" = x -o "x$(BASH)" = xno; then \
		printf 'SKIP: bash is not available; converter tests skipped\n'; \
		exit 0; \
	fi
	@if test -z "$(CONVERTER_TEST_SCRIPT_PATHS)"; then \
		printf 'SKIP: no converter tests are enabled\n'; \
		exit 0; \
	fi
	@mkdir -p $(builddir)/tmp/logs
	@rm -f $(builddir)/test-suite.log
	@overall_status=0; \
	for test_script in $(CONVERTER_TEST_SCRIPT_PATHS); do \
		name=`basename $$test_script`; \
		log_file=$(builddir)/$$name.log; \
		trs_file=$(builddir)/$$name.trs; \
		rm -f "$$log_file" "$$trs_file"; \
		if ! $(CONVERTER_TAP_DRIVER) --test-name "$$name" --log-file "$$log_file" --trs-file "$$trs_file" --expect-failure no --color-tests no --enable-hard-errors yes --comments yes -- env $(CONVERTER_TEST_ENV_VARS) $(CONVERTER_TAP_COMPILER) "$$test_script"; then \
			overall_status=1; \
		fi; \
		cat "$$trs_file" >>$(builddir)/test-suite.log; \
	done; \
	exit $$overall_status
