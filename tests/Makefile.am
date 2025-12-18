if COND_GCOV
MAYBE_COVERAGE = --coverage
endif
CLI_TESTS = cli/t/test_0001_01_invalid_positional.t \
       cli/t/test_0001_02_invalid_option_arguments.t \
       cli/t/test_0001_03_invalid_option_combinations.t \
       cli/t/test_0002_stdin.t \
       cli/t/test_0003_information.t \
       cli/t/test_0004_conversion_options_01.t \
       cli/t/test_0004_conversion_options_02.t \
       cli/t/test_0004_conversion_options_03.t \
       cli/t/test_0004_conversion_options_04.t \
       cli/t/test_0005_dcs_arguments.t \
       cli/t/test_0006_dcs_variations.t \
       cli/t/test_0007_animation.t \
       cli/t/test_0008_progressive_jpeg.t \
       cli/t/test_0009_pngsuite.t \
       cli/t/test_0010_curl.t \
       cli/t/test_0011_basic.t \
       cli/t/test_0012_regressions.t \
       cli/t/test_0013_documentation.t \
       cli/t/test_0014_option_matching.t \
       cli/t/test_0015_argument_shift.t \
       cli/t/test_0016_assessment_spool.t \
       cli/t/test_0017_clipboard.t \
       cli/t/test_0018_lsqa_regression.t \
       cli/t/test_0019_python_wheel.t \
       cli/t/test_0020_mapfile_io.t \
       cli/t/test_0021_pipeline_planner.t \
       cli/t/test_0021_palette_fallback.t \
       cli/t/test_0022_resize_precision_mode.t

PYTHON_TESTS = python/t/test_0001_python_bindings.t \
       python/t/test_0002_python_api_formats.t \
       python/t/test_0003_python_api_options.t \
       python/t/test_0004_python_api_errors.t \
       python/t/test_0005_python_resource_management.t

FILTER_TESTS = filter/t/test_3000_filter_clip.t \
       filter/t/test_3001_filter_sample.t \
       filter/t/test_3002_filter_resize.t \
       filter/t/test_3003_filter_colors.t \
       filter/t/test_3004_filter_lookup.t \
       filter/t/test_3005_filter_final_merge.t \
       filter/t/test_3006_filter_vpte.t \
       filter/t/test_3007_filter_load.t \
       filter/t/test_3008_filter_dither.t \
       filter/t/test_3009_filter_encode.t

FILTER_TEST_BINARIES = \
       filter/filter_tests \
       filter/filter_sample_tests \
       filter/filter_resize_tests \
       filter/filter_colors_tests \
       filter/filter_lookup_tests \
       filter/filter_final_merge_tests \
       filter/filter_vpte_tests \
       filter/filter_load_tests \
       filter/filter_dither_tests \
       filter/filter_encode_tests

TESTS = $(CLI_TESTS) $(PYTHON_TESTS) $(FILTER_TESTS)
COPY_TESTS = $(CLI_TESTS) $(PYTHON_TESTS) $(FILTER_TESTS)
check_PROGRAMS =

if ENABLE_GDK_PIXBUF_LOADER
TESTS += gdk-pixbuf-loader/test-gdk-pixbuf-loader
check_PROGRAMS += gdk-pixbuf-loader/test-gdk-pixbuf-loader
gdk_pixbuf_loader_test_gdk_pixbuf_loader_SOURCES = \
    gdk-pixbuf-loader/test-gdk-pixbuf-loader.c \
    $(top_srcdir)/gdk-pixbuf-loader/sixel-pixbuf-loader.c
gdk_pixbuf_loader_test_gdk_pixbuf_loader_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS) \
        -DGDK_PIXBUF_ENABLE_BACKEND
gdk_pixbuf_loader_test_gdk_pixbuf_loader_LDADD = \
        $(top_builddir)/src/libsixel.la \
        $(GDK_PIXBUF_LIBS)
endif
TEST_EXTENSIONS = .t

HELPER_FILES = cli/t/converters-common.t \
       python/lib/common.sh

ARTIFACT_ROOT = $(abs_builddir)/_artifacts
AM_TESTS_ENVIRONMENT = ARTIFACT_ROOT='$(ARTIFACT_ROOT)'; export ARTIFACT_ROOT;
AM_TESTS_ENVIRONMENT += TOP_SRCDIR='$(abs_top_srcdir)'; export TOP_SRCDIR;
AM_TESTS_ENVIRONMENT += TOP_BUILDDIR='$(abs_top_builddir)'; export TOP_BUILDDIR;
AM_TESTS_ENVIRONMENT += CC='$(CC)'; export CC;
AM_TESTS_ENVIRONMENT += CROSS_COMPILING='@cross_compiling@'; \
       export CROSS_COMPILING;
FILTER_TEST_CPPFLAGS = $(AM_CPPFLAGS) \
       -I$(top_builddir) -I$(top_builddir)/include \
       -I$(top_srcdir)/include -I$(top_srcdir)/src \
       -I$(top_srcdir)/tests/filter
FILTER_TEST_CFLAGS = $(AM_CFLAGS) $(MAYBE_COVERAGE)
FILTER_TEST_LDFLAGS = $(AM_LDFLAGS) $(MAYBE_COVERAGE)
FILTER_TEST_LDADD = $(top_builddir)/src/libsixel.la $(LIBS)

check_PROGRAMS += $(FILTER_TEST_BINARIES)

filter_filter_tests_SOURCES = filter/filter_tests.c
filter_filter_tests_CPPFLAGS = $(FILTER_TEST_CPPFLAGS)
filter_filter_tests_CFLAGS = $(FILTER_TEST_CFLAGS)
filter_filter_tests_LDFLAGS = $(FILTER_TEST_LDFLAGS)
filter_filter_tests_LDADD = $(FILTER_TEST_LDADD)

filter_filter_sample_tests_SOURCES = filter/filter_sample_tests.c
filter_filter_sample_tests_CPPFLAGS = $(FILTER_TEST_CPPFLAGS)
filter_filter_sample_tests_CFLAGS = $(FILTER_TEST_CFLAGS)
filter_filter_sample_tests_LDFLAGS = $(FILTER_TEST_LDFLAGS)
filter_filter_sample_tests_LDADD = $(FILTER_TEST_LDADD)

filter_filter_resize_tests_SOURCES = filter/filter_resize_tests.c
filter_filter_resize_tests_CPPFLAGS = $(FILTER_TEST_CPPFLAGS)
filter_filter_resize_tests_CFLAGS = $(FILTER_TEST_CFLAGS)
filter_filter_resize_tests_LDFLAGS = $(FILTER_TEST_LDFLAGS)
filter_filter_resize_tests_LDADD = $(FILTER_TEST_LDADD)

filter_filter_colors_tests_SOURCES = filter/filter_colors_tests.c
filter_filter_colors_tests_CPPFLAGS = $(FILTER_TEST_CPPFLAGS)
filter_filter_colors_tests_CFLAGS = $(FILTER_TEST_CFLAGS)
filter_filter_colors_tests_LDFLAGS = $(FILTER_TEST_LDFLAGS)
filter_filter_colors_tests_LDADD = $(FILTER_TEST_LDADD)

filter_filter_lookup_tests_SOURCES = filter/filter_lookup_tests.c
filter_filter_lookup_tests_CPPFLAGS = $(FILTER_TEST_CPPFLAGS)
filter_filter_lookup_tests_CFLAGS = $(FILTER_TEST_CFLAGS)
filter_filter_lookup_tests_LDFLAGS = $(FILTER_TEST_LDFLAGS)
filter_filter_lookup_tests_LDADD = $(FILTER_TEST_LDADD)

filter_filter_final_merge_tests_SOURCES = filter/filter_final_merge_tests.c
filter_filter_final_merge_tests_CPPFLAGS = $(FILTER_TEST_CPPFLAGS)
filter_filter_final_merge_tests_CFLAGS = $(FILTER_TEST_CFLAGS)
filter_filter_final_merge_tests_LDFLAGS = $(FILTER_TEST_LDFLAGS)
filter_filter_final_merge_tests_LDADD = $(FILTER_TEST_LDADD)

filter_filter_vpte_tests_SOURCES = filter/filter_vpte_tests.c
filter_filter_vpte_tests_CPPFLAGS = $(FILTER_TEST_CPPFLAGS)
filter_filter_vpte_tests_CFLAGS = $(FILTER_TEST_CFLAGS)
filter_filter_vpte_tests_LDFLAGS = $(FILTER_TEST_LDFLAGS)
filter_filter_vpte_tests_LDADD = $(FILTER_TEST_LDADD)

filter_filter_load_tests_SOURCES = filter/filter_load_tests.c
filter_filter_load_tests_CPPFLAGS = $(FILTER_TEST_CPPFLAGS)
filter_filter_load_tests_CFLAGS = $(FILTER_TEST_CFLAGS)
filter_filter_load_tests_LDFLAGS = $(FILTER_TEST_LDFLAGS)
filter_filter_load_tests_LDADD = $(FILTER_TEST_LDADD)

filter_filter_dither_tests_SOURCES = filter/filter_dither_tests.c
filter_filter_dither_tests_CPPFLAGS = $(FILTER_TEST_CPPFLAGS)
filter_filter_dither_tests_CFLAGS = $(FILTER_TEST_CFLAGS)
filter_filter_dither_tests_LDFLAGS = $(FILTER_TEST_LDFLAGS)
filter_filter_dither_tests_LDADD = $(FILTER_TEST_LDADD)

filter_filter_encode_tests_SOURCES = filter/filter_encode_tests.c
filter_filter_encode_tests_CPPFLAGS = $(FILTER_TEST_CPPFLAGS)
filter_filter_encode_tests_CFLAGS = $(FILTER_TEST_CFLAGS)
filter_filter_encode_tests_LDFLAGS = $(FILTER_TEST_LDFLAGS)
filter_filter_encode_tests_LDADD = $(FILTER_TEST_LDADD)
  
TEST_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/build-aux/tap-driver.sh
AM_TEST_LOG_DRIVER_FLAGS = --comments
LOG_COMPILER = $(builddir)/tools/with-timeout.sh
AM_TEST_TIMEOUT = 300
AM_LOG_FLAGS = --timeout=$(AM_TEST_TIMEOUT)

EXTRA_DIST = $(TESTS) $(HELPER_FILES) tools/with-timeout.sh \
             lsqa_regression.sh \
             data/baseline/*.json data/inputs/formats/* \
             data/resolutions/*.png data/corrupted/* \
             filter/filter_tests.c filter/filter_sample_tests.c \
             filter/filter_resize_tests.c filter/filter_colors_tests.c \
             filter/filter_lookup_tests.c filter/filter_final_merge_tests.c \
             filter/filter_vpte_tests.c filter/filter_load_tests.c \
             filter/filter_dither_tests.c filter/filter_encode_tests.c \
             filter/filter_test_common.h
# Avoid deleting the in-tree timeout wrapper when building in the source
# directory. The wrapper is regenerated for out-of-tree builds, so remove it
# only in that case to prevent accidental loss from "make clean".
CLEANFILES =

# Avoid deleting in-tree test scripts when "make clean" is invoked.
# Only remove copied test files in an out-of-tree build so that the
# distributed sources remain intact for developers and package builds.

# Keep this recipe tab-indented; GNU make requires tabs for commands.
clean-local:
	@if test "$(abs_builddir)" != "$(abs_srcdir)"; then \\
		rm -f $(LOG_COMPILER) $(COPY_TESTS) $(HELPER_FILES); \\
	fi

# Keep this recipe tab-indented; GNU make requires tabs for commands.
check-local: $(LOG_COMPILER) $(COPY_TESTS) $(HELPER_FILES)

# Keep this recipe tab-indented; GNU make requires tabs for commands.
$(LOG_COMPILER): $(srcdir)/tools/with-timeout.sh
	$(MKDIR_P) $(builddir)/tools
	$(AM_V_GEN)cp $(srcdir)/tools/with-timeout.sh $(LOG_COMPILER)

# Keep this recipe tab-indented; GNU make requires tabs for commands.
$(COPY_TESTS) $(HELPER_FILES):
	$(MKDIR_P) $(builddir)/$(dir $@)
	$(AM_V_GEN){ if test -f $(srcdir)/$@; then \
		cp $(srcdir)/$@ $(builddir)/$@; \
	fi; }
