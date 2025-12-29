if COND_GCOV
MAYBE_COVERAGE = --coverage
endif
if CFLAGS_HAVE_COVERAGE
MAYBE_COVERAGE = --coverage
endif
SIXEL_SHEBANG_FILE = @SIXEL_SHEBANG_FILE@
SIXEL_SHEBANG_HELPER = $(top_srcdir)/build-aux/sixel-apply-shebang.sh
AMALGAMATION_SIXEL = $(top_builddir)/amalgamation/sixel.c
if WANT_AMALGAMATED_TOOLS
BUILT_SOURCES = $(AMALGAMATION_SIXEL)

# Keep this recipe tab-indented; GNU make requires tabs for commands.
$(AMALGAMATION_SIXEL): $(top_builddir)/amalgamation/Makefile
	$(AM_V_GEN)$(MAKE) -C $(top_builddir)/converters completion_embed.h
	$(AM_V_GEN)$(MAKE) -C $(top_builddir)/amalgamation sixel.c
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
       cli/t/test_0022_resize_precision_mode.t \
       cli/t/test_0023_cli_token_is_known_option.t \
       cli/t/test_0024_cli_option_requires_argument.t \
       cli/t/test_0025_cli_guard_missing_argument.t \
       cli/t/test_0026_sixel2png_version_help.t \
       cli/t/test_0027_sixel2png_missing_args.t \
       cli/t/test_0028_sixel2png_unknown_option.t \
       cli/t/test_0029_sixel2png_invalid_decoder_value.t \
       cli/t/test_0030_sixel2png_default_output.t

CORE_TESTS = probe/test_probe_parse.t \
       palette/test_palette_kmeans_init.t

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

TESTS = $(CLI_TESTS) $(CORE_TESTS) $(PYTHON_TESTS) $(FILTER_TESTS)
COPY_TESTS = $(CLI_TESTS) $(CORE_TESTS) $(PYTHON_TESTS) $(FILTER_TESTS)
check_PROGRAMS = \
    cli/test_0023_cli_token_is_known_option \
    cli/test_0024_cli_option_requires_argument \
    cli/test_0025_cli_guard_missing_argument \
    probe/test_probe_parse \
    palette/test_palette_kmeans_init

cli_test_0023_cli_token_is_known_option_SOURCES = \
    cli/test_0023_cli_token_is_known_option.c \
    $(top_srcdir)/converters/cli.c
cli_test_0023_cli_token_is_known_option_CPPFLAGS = $(AM_CPPFLAGS) \
    -I$(top_builddir) -I$(top_builddir)/include \
    -I$(top_srcdir) -I$(top_srcdir)/include \
    -I$(top_srcdir)/converters
cli_test_0023_cli_token_is_known_option_LDADD = \
    $(top_builddir)/src/libsixel.la

cli_test_0024_cli_option_requires_argument_SOURCES = \
    cli/test_0024_cli_option_requires_argument.c \
    $(top_srcdir)/converters/cli.c
cli_test_0024_cli_option_requires_argument_CPPFLAGS = $(AM_CPPFLAGS) \
    -I$(top_builddir) -I$(top_builddir)/include \
    -I$(top_srcdir) -I$(top_srcdir)/include \
    -I$(top_srcdir)/converters
cli_test_0024_cli_option_requires_argument_LDADD = \
    $(top_builddir)/src/libsixel.la

cli_test_0025_cli_guard_missing_argument_SOURCES = \
    cli/test_0025_cli_guard_missing_argument.c \
    $(top_srcdir)/converters/cli.c
cli_test_0025_cli_guard_missing_argument_CPPFLAGS = $(AM_CPPFLAGS) \
    -I$(top_builddir) -I$(top_builddir)/include \
    -I$(top_srcdir) -I$(top_srcdir)/include \
    -I$(top_srcdir)/converters
cli_test_0025_cli_guard_missing_argument_LDADD = \
    $(top_builddir)/src/libsixel.la

probe_test_probe_parse_SOURCES = \
    probe/test_probe_parse.c
probe_test_probe_parse_CPPFLAGS = $(AM_CPPFLAGS) \
    -I$(top_builddir) -I$(top_builddir)/include \
    -I$(top_srcdir) -I$(top_srcdir)/include \
    -I$(top_srcdir)/src
probe_test_probe_parse_LDADD = $(top_builddir)/src/libsixel.la

palette_test_palette_kmeans_init_SOURCES = \
    palette/test_palette_kmeans_init.c
palette_test_palette_kmeans_init_CPPFLAGS = $(AM_CPPFLAGS) \
    -I$(top_builddir) -I$(top_builddir)/include \
    -I$(top_srcdir) -I$(top_srcdir)/include \
    -I$(top_srcdir)/src
palette_test_palette_kmeans_init_LDADD = $(top_builddir)/src/libsixel.la

if ENABLE_GDK_PIXBUF_LOADER
TESTS += gdk-pixbuf-loader/test-gdk-pixbuf-loader \
    gdk-pixbuf-loader/test_0001_incremental_load \
    gdk-pixbuf-loader/test_0002_corrupt_data \
    gdk-pixbuf-loader/test_0003_propagate_error \
    gdk-pixbuf-loader/test_0004_context_free
check_PROGRAMS += gdk-pixbuf-loader/test-gdk-pixbuf-loader \
    gdk-pixbuf-loader/test_0001_incremental_load \
    gdk-pixbuf-loader/test_0002_corrupt_data \
    gdk-pixbuf-loader/test_0003_propagate_error \
    gdk-pixbuf-loader/test_0004_context_free
gdk_pixbuf_loader_test_gdk_pixbuf_loader_SOURCES = \
    gdk-pixbuf-loader/test-gdk-pixbuf-loader.c \
    $(top_srcdir)/gdk-pixbuf-loader/sixel-pixbuf-loader.c
gdk_pixbuf_loader_test_0001_incremental_load_SOURCES = \
    gdk-pixbuf-loader/test_0001_incremental_load.c \
    $(top_srcdir)/gdk-pixbuf-loader/sixel-pixbuf-loader.c
gdk_pixbuf_loader_test_0002_corrupt_data_SOURCES = \
    gdk-pixbuf-loader/test_0002_corrupt_data.c \
    $(top_srcdir)/gdk-pixbuf-loader/sixel-pixbuf-loader.c
gdk_pixbuf_loader_test_0003_propagate_error_SOURCES = \
    gdk-pixbuf-loader/test_0003_propagate_error.c \
    $(top_srcdir)/gdk-pixbuf-loader/sixel-pixbuf-loader.c
gdk_pixbuf_loader_test_0004_context_free_SOURCES = \
    gdk-pixbuf-loader/test_0004_context_free.c \
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
gdk_pixbuf_loader_test_0001_incremental_load_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS) \
        -DGDK_PIXBUF_ENABLE_BACKEND \
        -DSIXEL_PIXBUF_LOADER_TESTING
gdk_pixbuf_loader_test_0002_corrupt_data_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS) \
        -DGDK_PIXBUF_ENABLE_BACKEND \
        -DSIXEL_PIXBUF_LOADER_TESTING
gdk_pixbuf_loader_test_0003_propagate_error_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS) \
        -DGDK_PIXBUF_ENABLE_BACKEND \
        -DSIXEL_PIXBUF_LOADER_TESTING
gdk_pixbuf_loader_test_0004_context_free_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS) \
        -DGDK_PIXBUF_ENABLE_BACKEND \
        -DSIXEL_PIXBUF_LOADER_TESTING
gdk_pixbuf_loader_test_0001_incremental_load_LDADD = \
        $(top_builddir)/src/libsixel.la \
        $(GDK_PIXBUF_LIBS)
gdk_pixbuf_loader_test_0002_corrupt_data_LDADD = \
        $(top_builddir)/src/libsixel.la \
        $(GDK_PIXBUF_LIBS)
gdk_pixbuf_loader_test_0003_propagate_error_LDADD = \
        $(top_builddir)/src/libsixel.la \
        $(GDK_PIXBUF_LIBS)
gdk_pixbuf_loader_test_0004_context_free_LDADD = \
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
#AM_TESTS_ENVIRONMENT += CC='$(CC)'; export CC;
AM_TESTS_ENVIRONMENT += CROSS_COMPILING='@cross_compiling@'; \
       export CROSS_COMPILING;
FILTER_COMMON_CPPFLAGS_VAL = $(AM_CPPFLAGS) \
       -I$(top_builddir) -I$(top_builddir)/include \
       -I$(top_srcdir)/include -I$(top_srcdir)/src \
       -I$(top_srcdir)/tests/filter
if WANT_AMALGAMATED_TOOLS
FILTER_COMMON_CFLAGS_VAL = $(AM_CFLAGS) \
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
        $(ONNXRUNTIME_CFLAGS) \
        @THREAD_CFLAGS@ \
        $(MAYBE_COVERAGE)
else
FILTER_COMMON_CFLAGS_VAL = $(AM_CFLAGS) \
        $(MAYBE_COVERAGE)
endif
FILTER_COMMON_LDFLAGS_VAL = $(AM_LDFLAGS) $(MAYBE_COVERAGE)
if WANT_AMALGAMATED_TOOLS
FILTER_COMMON_LDADD_VAL = $(LIBS) \
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
        $(ONNXRUNTIME_LIBS) \
        @THREAD_LIBS@
else
FILTER_COMMON_LDADD_VAL = $(top_builddir)/src/libsixel.la $(LIBS)
endif

FILTER_AMALGAMATION_CPPFLAGS_COMMON = $(FILTER_COMMON_CPPFLAGS_VAL)
FILTER_AMALGAMATION_CFLAGS_COMMON = $(FILTER_COMMON_CFLAGS_VAL)
FILTER_AMALGAMATION_LDFLAGS_COMMON = $(FILTER_COMMON_LDFLAGS_VAL)
if AMALGAMATION_NEEDS_OBJC
FILTER_AMALGAMATION_CPPFLAGS_COMMON += -x objective-c
endif
if WANT_AMALGAMATED_TOOLS
FILTER_AMALGAMATION_LDADD_COMMON = $(LIBS) \
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
        $(ONNXRUNTIME_LIBS) \
        @THREAD_LIBS@
else
FILTER_AMALGAMATION_LDADD_COMMON = $(top_builddir)/src/libsixel.la $(LIBS)
endif

check_PROGRAMS += $(FILTER_TEST_BINARIES)

if WANT_AMALGAMATED_TOOLS
nodist_filter_filter_tests_SOURCES = $(AMALGAMATION_SIXEL)
filter_filter_tests_CPPFLAGS = $(FILTER_AMALGAMATION_CPPFLAGS_COMMON) -DBUILD_FILTER_TESTS=1
filter_filter_tests_CFLAGS = $(FILTER_AMALGAMATION_CFLAGS_COMMON)
filter_filter_tests_LDFLAGS = $(FILTER_AMALGAMATION_LDFLAGS_COMMON)
filter_filter_tests_LDADD = $(FILTER_AMALGAMATION_LDADD_COMMON)
else
filter_filter_tests_SOURCES = filter/filter_tests.c
filter_filter_tests_CPPFLAGS = $(FILTER_COMMON_CPPFLAGS_VAL)
filter_filter_tests_CFLAGS = $(FILTER_COMMON_CFLAGS_VAL)
filter_filter_tests_LDFLAGS = $(FILTER_COMMON_LDFLAGS_VAL)
filter_filter_tests_LDADD = $(FILTER_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_filter_sample_tests_SOURCES = $(AMALGAMATION_SIXEL)
filter_filter_sample_tests_CPPFLAGS = $(FILTER_AMALGAMATION_CPPFLAGS_COMMON) -DBUILD_FILTER_SAMPLE_TESTS=1
filter_filter_sample_tests_CFLAGS = $(FILTER_AMALGAMATION_CFLAGS_COMMON)
filter_filter_sample_tests_LDFLAGS = $(FILTER_AMALGAMATION_LDFLAGS_COMMON)
filter_filter_sample_tests_LDADD = $(FILTER_AMALGAMATION_LDADD_COMMON)
else
filter_filter_sample_tests_SOURCES = filter/filter_sample_tests.c
filter_filter_sample_tests_CPPFLAGS = $(FILTER_COMMON_CPPFLAGS_VAL)
filter_filter_sample_tests_CFLAGS = $(FILTER_COMMON_CFLAGS_VAL)
filter_filter_sample_tests_LDFLAGS = $(FILTER_COMMON_LDFLAGS_VAL)
filter_filter_sample_tests_LDADD = $(FILTER_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_filter_resize_tests_SOURCES = $(AMALGAMATION_SIXEL)
filter_filter_resize_tests_CPPFLAGS = $(FILTER_AMALGAMATION_CPPFLAGS_COMMON) -DBUILD_FILTER_RESIZE_TESTS=1
filter_filter_resize_tests_CFLAGS = $(FILTER_AMALGAMATION_CFLAGS_COMMON)
filter_filter_resize_tests_LDFLAGS = $(FILTER_AMALGAMATION_LDFLAGS_COMMON)
filter_filter_resize_tests_LDADD = $(FILTER_AMALGAMATION_LDADD_COMMON)
else
filter_filter_resize_tests_SOURCES = filter/filter_resize_tests.c
filter_filter_resize_tests_CPPFLAGS = $(FILTER_COMMON_CPPFLAGS_VAL)
filter_filter_resize_tests_CFLAGS = $(FILTER_COMMON_CFLAGS_VAL)
filter_filter_resize_tests_LDFLAGS = $(FILTER_COMMON_LDFLAGS_VAL)
filter_filter_resize_tests_LDADD = $(FILTER_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_filter_colors_tests_SOURCES = $(AMALGAMATION_SIXEL)
filter_filter_colors_tests_CPPFLAGS = $(FILTER_AMALGAMATION_CPPFLAGS_COMMON) -DBUILD_FILTER_COLORS_TESTS=1
filter_filter_colors_tests_CFLAGS = $(FILTER_AMALGAMATION_CFLAGS_COMMON)
filter_filter_colors_tests_LDFLAGS = $(FILTER_AMALGAMATION_LDFLAGS_COMMON)
filter_filter_colors_tests_LDADD = $(FILTER_AMALGAMATION_LDADD_COMMON)
else
filter_filter_colors_tests_SOURCES = filter/filter_colors_tests.c
filter_filter_colors_tests_CPPFLAGS = $(FILTER_COMMON_CPPFLAGS_VAL)
filter_filter_colors_tests_CFLAGS = $(FILTER_COMMON_CFLAGS_VAL)
filter_filter_colors_tests_LDFLAGS = $(FILTER_COMMON_LDFLAGS_VAL)
filter_filter_colors_tests_LDADD = $(FILTER_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_filter_lookup_tests_SOURCES = $(AMALGAMATION_SIXEL)
filter_filter_lookup_tests_CPPFLAGS = $(FILTER_AMALGAMATION_CPPFLAGS_COMMON) -DBUILD_FILTER_LOOKUP_TESTS=1
filter_filter_lookup_tests_CFLAGS = $(FILTER_AMALGAMATION_CFLAGS_COMMON)
filter_filter_lookup_tests_LDFLAGS = $(FILTER_AMALGAMATION_LDFLAGS_COMMON)
filter_filter_lookup_tests_LDADD = $(FILTER_AMALGAMATION_LDADD_COMMON)
else
filter_filter_lookup_tests_SOURCES = filter/filter_lookup_tests.c
filter_filter_lookup_tests_CPPFLAGS = $(FILTER_COMMON_CPPFLAGS_VAL)
filter_filter_lookup_tests_CFLAGS = $(FILTER_COMMON_CFLAGS_VAL)
filter_filter_lookup_tests_LDFLAGS = $(FILTER_COMMON_LDFLAGS_VAL)
filter_filter_lookup_tests_LDADD = $(FILTER_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_filter_final_merge_tests_SOURCES = $(AMALGAMATION_SIXEL)
filter_filter_final_merge_tests_CPPFLAGS = $(FILTER_AMALGAMATION_CPPFLAGS_COMMON) -DBUILD_FILTER_FINAL_MERGE_TESTS=1
filter_filter_final_merge_tests_CFLAGS = $(FILTER_AMALGAMATION_CFLAGS_COMMON)
filter_filter_final_merge_tests_LDFLAGS = $(FILTER_AMALGAMATION_LDFLAGS_COMMON)
filter_filter_final_merge_tests_LDADD = $(FILTER_AMALGAMATION_LDADD_COMMON)
else
filter_filter_final_merge_tests_SOURCES = filter/filter_final_merge_tests.c
filter_filter_final_merge_tests_CPPFLAGS = $(FILTER_COMMON_CPPFLAGS_VAL)
filter_filter_final_merge_tests_CFLAGS = $(FILTER_COMMON_CFLAGS_VAL)
filter_filter_final_merge_tests_LDFLAGS = $(FILTER_COMMON_LDFLAGS_VAL)
filter_filter_final_merge_tests_LDADD = $(FILTER_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_filter_vpte_tests_SOURCES = $(AMALGAMATION_SIXEL)
filter_filter_vpte_tests_CPPFLAGS = $(FILTER_AMALGAMATION_CPPFLAGS_COMMON) -DBUILD_FILTER_VPTE_TESTS=1
filter_filter_vpte_tests_CFLAGS = $(FILTER_AMALGAMATION_CFLAGS_COMMON)
filter_filter_vpte_tests_LDFLAGS = $(FILTER_AMALGAMATION_LDFLAGS_COMMON)
filter_filter_vpte_tests_LDADD = $(FILTER_AMALGAMATION_LDADD_COMMON)
else
filter_filter_vpte_tests_SOURCES = filter/filter_vpte_tests.c
filter_filter_vpte_tests_CPPFLAGS = $(FILTER_COMMON_CPPFLAGS_VAL)
filter_filter_vpte_tests_CFLAGS = $(FILTER_COMMON_CFLAGS_VAL)
filter_filter_vpte_tests_LDFLAGS = $(FILTER_COMMON_LDFLAGS_VAL)
filter_filter_vpte_tests_LDADD = $(FILTER_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_filter_load_tests_SOURCES = $(AMALGAMATION_SIXEL)
filter_filter_load_tests_CPPFLAGS = $(FILTER_AMALGAMATION_CPPFLAGS_COMMON) -DBUILD_FILTER_LOAD_TESTS=1
filter_filter_load_tests_CFLAGS = $(FILTER_AMALGAMATION_CFLAGS_COMMON)
filter_filter_load_tests_LDFLAGS = $(FILTER_AMALGAMATION_LDFLAGS_COMMON)
filter_filter_load_tests_LDADD = $(FILTER_AMALGAMATION_LDADD_COMMON)
else
filter_filter_load_tests_SOURCES = filter/filter_load_tests.c
filter_filter_load_tests_CPPFLAGS = $(FILTER_COMMON_CPPFLAGS_VAL)
filter_filter_load_tests_CFLAGS = $(FILTER_COMMON_CFLAGS_VAL)
filter_filter_load_tests_LDFLAGS = $(FILTER_COMMON_LDFLAGS_VAL)
filter_filter_load_tests_LDADD = $(FILTER_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_filter_dither_tests_SOURCES = $(AMALGAMATION_SIXEL)
filter_filter_dither_tests_CPPFLAGS = $(FILTER_AMALGAMATION_CPPFLAGS_COMMON) -DBUILD_FILTER_DITHER_TESTS=1
filter_filter_dither_tests_CFLAGS = $(FILTER_AMALGAMATION_CFLAGS_COMMON)
filter_filter_dither_tests_LDFLAGS = $(FILTER_AMALGAMATION_LDFLAGS_COMMON)
filter_filter_dither_tests_LDADD = $(FILTER_AMALGAMATION_LDADD_COMMON)
else
filter_filter_dither_tests_SOURCES = filter/filter_dither_tests.c
filter_filter_dither_tests_CPPFLAGS = $(FILTER_COMMON_CPPFLAGS_VAL)
filter_filter_dither_tests_CFLAGS = $(FILTER_COMMON_CFLAGS_VAL)
filter_filter_dither_tests_LDFLAGS = $(FILTER_COMMON_LDFLAGS_VAL)
filter_filter_dither_tests_LDADD = $(FILTER_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_filter_encode_tests_SOURCES = $(AMALGAMATION_SIXEL)
filter_filter_encode_tests_CPPFLAGS = $(FILTER_AMALGAMATION_CPPFLAGS_COMMON) -DBUILD_FILTER_ENCODE_TESTS=1
filter_filter_encode_tests_CFLAGS = $(FILTER_AMALGAMATION_CFLAGS_COMMON)
filter_filter_encode_tests_LDFLAGS = $(FILTER_AMALGAMATION_LDFLAGS_COMMON)
filter_filter_encode_tests_LDADD = $(FILTER_AMALGAMATION_LDADD_COMMON)
else
filter_filter_encode_tests_SOURCES = filter/filter_encode_tests.c
filter_filter_encode_tests_CPPFLAGS = $(FILTER_COMMON_CPPFLAGS_VAL)
filter_filter_encode_tests_CFLAGS = $(FILTER_COMMON_CFLAGS_VAL)
filter_filter_encode_tests_LDFLAGS = $(FILTER_COMMON_LDFLAGS_VAL)
filter_filter_encode_tests_LDADD = $(FILTER_COMMON_LDADD_VAL)
endif
  
TEST_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/build-aux/tap-driver.sh
AM_TEST_LOG_DRIVER_FLAGS = --comments
LOG_COMPILER = $(builddir)/tools/with-timeout.sh
AM_TEST_TIMEOUT = 300
AM_LOG_FLAGS = --timeout=$(AM_TEST_TIMEOUT)

BASELINE_JSON = data/baseline/grayscale.json \
       data/baseline/hd_landscape.json \
       data/baseline/palette.json \
       data/baseline/panorama_tall.json \
       data/baseline/panorama_wide.json \
       data/baseline/quarter_square.json \
       data/baseline/rgb.json \
       data/baseline/svga_landscape.json \
       data/baseline/tiny_square.json

INPUT_FORMAT_FILES = data/inputs/formats/grayscale.jpg \
       data/inputs/formats/palette.png \
       data/inputs/formats/rgb.png

RESOLUTION_IMAGES = data/resolutions/hd_landscape.png \
       data/resolutions/panorama_tall.png \
       data/resolutions/panorama_wide.png \
       data/resolutions/quarter_square.png \
       data/resolutions/svga_landscape.png \
       data/resolutions/tiny_square.png

CORRUPTED_INPUTS = data/corrupted/metadata_noise.jpg \
       data/corrupted/truncated.png

ISSUE_INPUTS = issue/166/poc \
       issue/167/poc \
       issue/200/POC_img2sixel_heap_buffer_overflow

EXTRA_DIST = $(TESTS) $(HELPER_FILES) tools/with-timeout.sh \
             lsqa_regression.sh \
             $(BASELINE_JSON) $(INPUT_FORMAT_FILES) \
             $(RESOLUTION_IMAGES) $(CORRUPTED_INPUTS) $(ISSUE_INPUTS) \
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
	@if test "$(abs_builddir)" != "$(abs_srcdir)"; then \
	rm -f $(LOG_COMPILER) $(COPY_TESTS) $(HELPER_FILES); \
	fi
	rm -rf $(ARTIFACT_ROOT)
	rm -f $(builddir)/*_metrics.json

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


if WITH_SHEBANG_FILE
SIXEL_SHEBANG_TARGETS = $(check_PROGRAMS)

apply-shebang-check: $(SIXEL_SHEBANG_TARGETS)
	@if test -n "$(SIXEL_SHEBANG_TARGETS)"; then \
		$(SIXEL_SHEBANG_HELPER) "$(SIXEL_SHEBANG_FILE)" \
			$(SIXEL_SHEBANG_TARGETS); \
	fi

check-am: apply-shebang-check
endif
