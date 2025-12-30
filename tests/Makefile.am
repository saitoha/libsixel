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
CLI_TESTS = cli-core/t/0001_invalid_positional.t \
    cli-core/t/0002_invalid_option_arguments.t \
    cli-core/t/0003_invalid_option_combinations.t \
    cli-core/t/0004_stdin.t \
    cli-core/t/0005_help.t \
    cli-core/t/0005_version.t \
    cli-core/t/0006_basic.t \
    cli-core/t/0007_option_matching.t \
    cli-core/t/0008_argument_shift.t \
    cli-core/t/0009_cli_token_is_known_option.t \
    cli-core/t/0010_cli_option_requires_argument.t \
    cli-core/t/0011_cli_guard_missing_argument.t \
    converter-options/t/0001_conversion_options_01.t \
    converter-options/t/0002_conversion_options_02.t \
    converter-options/t/0003_conversion_options_03.t \
    converter-options/t/0004_conversion_options_04.t \
    decoder/t/0001_dcs_arguments.t \
    decoder/t/0002_tab_separated_colour_introducers.t \
    decoder/t/0003_oversized_dcs_geometry.t \
    animation/t/0001_disable_update_mode.t \
    animation/t/0002_static_frame_render.t \
    animation/t/0003_update_and_static_frame.t \
    animation/t/0004_split_sequence_atkinson.t \
    loader/t/0001_progressive_jpeg.t \
    loader/t/0002_pngsuite_basic_convert.t \
    loader/t/0003_pngsuite_basic_width32.t \
    loader/t/0004_pngsuite_basic_cropped.t \
    loader/t/0005_pngsuite_background_convert.t \
    loader/t/0006_pngsuite_background_white.t \
    loader/t/0007_pngsuite_background_width32_white.t \
    network/t/0001_curl_invalid_file_url.t \
    network/t/0002_curl_invalid_https_url.t \
    network/t/0003_curl_file_scheme.t \
    network/t/0004_curl_self_signed_blocked.t \
    network/t/0005_curl_self_signed_with_k.t \
    regression/t/0001_issue167_empty_bg.t \
    regression/t/0002_issue167_height.t \
    regression/t/0003_issue166_width.t \
    regression/t/0004_issue200_heap_overflow.t \
    regression/t/0005_lsqa_regression.t \
    docs/t/0001_help_vs_man.t \
    docs/t/0002_man_vs_bash_completion.t \
    docs/t/0003_bash_vs_zsh_completion.t \
    assessment/t/0001_spool.t \
    clipboard/t/0001_clipboard_basic.t \
    packaging/t/0001_python_wheel_install.t \
    packaging/t/0002_python_wheel_roundtrip.t \
    mapfile/t/0001_mapfile_export_act.t \
    mapfile/t/0002_mapfile_export_pal_default.t \
    mapfile/t/0003_mapfile_export_pal_stdout.t \
    mapfile/t/0004_mapfile_export_riff.t \
    mapfile/t/0005_mapfile_export_gpl.t \
    mapfile/t/0006_mapfile_import_gpl_prefixed.t \
    mapfile/t/0007_mapfile_import_act_extension.t \
    mapfile/t/0008_mapfile_import_riff_explicit.t \
    mapfile/t/0009_mapfile_import_stdin.t \
    pipeline/t/0001_pipeline_planner_baseline_run.t \
    pipeline/t/0002_pipeline_planner_baseline_summary.t \
    pipeline/t/0003_pipeline_planner_baseline_threads.t \
    pipeline/t/0004_pipeline_planner_overrides_run.t \
    pipeline/t/0005_pipeline_planner_overrides_summary.t \
    pipeline/t/0006_pipeline_planner_overrides_threads.t \
    resize/t/0001_resize_precision_mode_run.t \
    resize/t/0002_resize_precision_mode_scaler_input.t \
    resize/t/0003_resize_precision_mode_post_colorspace.t \
    sixel2png/t/0001_version.t \
    sixel2png/t/0002_help.t \
    sixel2png/t/0002_missing_args.t \
    sixel2png/t/0003_unknown_option.t \
    sixel2png/t/0004_invalid_decoder_value.t \
    sixel2png/t/0005_default_output.t

PROBE_TESTS = probe/t/0001_probe_parse.t

PROBE_TESTS_BINARIES = probe/0001_probe_parse

PALETTE_TESTS = palette/t/0001_kmeans_init_default_unset.t \
       palette/t/0002_kmeans_init_auto.t \
       palette/t/0003_kmeans_init_unknown.t \
       palette/t/0004_kmeans_init_pca.t \
       palette/t/0005_kmeans_init_cache.t \
       palette/t/0006_palette_fallback_conversion.t \
       palette/t/0007_palette_fallback_output.t

PALETTE_TESTS_BINARIES = palette/0001_kmeans_init

PYTHON_TESTS = python/t/0001_python_bindings.t \
       python/t/0002_python_api_formats.t \
       python/t/0003_python_api_options.t \
       python/t/0004_python_api_errors.t \
       python/t/0005_python_resource_management.t

FILTER_TESTS = filter/t/0001_filter_clip.t \
       filter/t/0002_filter_sample.t \
       filter/t/0003_filter_resize.t \
       filter/t/0004_filter_colors.t \
       filter/t/0005_filter_lookup.t \
       filter/t/0006_filter_final_merge.t \
       filter/t/0007_filter_vpte.t \
       filter/t/0008_filter_load.t \
       filter/t/0009_filter_dither.t \
       filter/t/0010_filter_encode.t

FILTER_TEST_BINARIES = \
       filter/0001_filter_clip \
       filter/0002_filter_sample \
       filter/0003_filter_resize \
       filter/0004_filter_colors \
       filter/0005_filter_lookup \
       filter/0006_filter_final_merge \
       filter/0007_filter_vpte \
       filter/0008_filter_load \
       filter/0009_filter_dither \
       filter/0010_filter_encode

TESTS = $(CLI_TESTS) $(PROBE_TESTS) $(PALETTE_TESTS) $(PYTHON_TESTS) $(FILTER_TESTS)
COPY_TESTS = $(CLI_TESTS) $(PROBE_TESTS) $(PALETTE_TESTS) $(PYTHON_TESTS) $(FILTER_TESTS)
check_PROGRAMS = \
    cli/0029_cli_token_is_known_option \
    cli/0030_cli_option_requires_argument \
    cli/0031_cli_guard_missing_argument

if ENABLE_GDK_PIXBUF_LOADER
TESTS += gdk-pixbuf-loader/0001_gdk_pixbuf_loader \
    gdk-pixbuf-loader/0002_incremental_load \
    gdk-pixbuf-loader/0003_corrupt_data \
    gdk-pixbuf-loader/0004_propagate_error \
    gdk-pixbuf-loader/0005_context_free
check_PROGRAMS += gdk-pixbuf-loader/0001_gdk_pixbuf_loader \
    gdk-pixbuf-loader/0002_incremental_load \
    gdk-pixbuf-loader/0003_corrupt_data \
    gdk-pixbuf-loader/0004_propagate_error \
    gdk-pixbuf-loader/0005_context_free
gdk_pixbuf_loader_0001_gdk_pixbuf_loader_SOURCES = \
    gdk-pixbuf-loader/0001_gdk_pixbuf_loader.c \
    $(top_srcdir)/gdk-pixbuf-loader/sixel-pixbuf-loader.c
gdk_pixbuf_loader_0002_incremental_load_SOURCES = \
    gdk-pixbuf-loader/0002_incremental_load.c \
    $(top_srcdir)/gdk-pixbuf-loader/sixel-pixbuf-loader.c
gdk_pixbuf_loader_0003_corrupt_data_SOURCES = \
    gdk-pixbuf-loader/0003_corrupt_data.c \
    $(top_srcdir)/gdk-pixbuf-loader/sixel-pixbuf-loader.c
gdk_pixbuf_loader_0004_propagate_error_SOURCES = \
    gdk-pixbuf-loader/0004_propagate_error.c \
    $(top_srcdir)/gdk-pixbuf-loader/sixel-pixbuf-loader.c
gdk_pixbuf_loader_0005_context_free_SOURCES = \
    gdk-pixbuf-loader/0005_context_free.c \
    $(top_srcdir)/gdk-pixbuf-loader/sixel-pixbuf-loader.c
gdk_pixbuf_loader_0001_gdk_pixbuf_loader_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS) \
        -DGDK_PIXBUF_ENABLE_BACKEND
gdk_pixbuf_loader_0001_gdk_pixbuf_loader_LDADD = \
        $(top_builddir)/src/libsixel.la \
        $(GDK_PIXBUF_LIBS)
gdk_pixbuf_loader_0002_incremental_load_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS) \
        -DGDK_PIXBUF_ENABLE_BACKEND \
        -DSIXEL_PIXBUF_LOADER_TESTING
gdk_pixbuf_loader_0003_corrupt_data_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS) \
        -DGDK_PIXBUF_ENABLE_BACKEND \
        -DSIXEL_PIXBUF_LOADER_TESTING
gdk_pixbuf_loader_0004_propagate_error_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS) \
        -DGDK_PIXBUF_ENABLE_BACKEND \
        -DSIXEL_PIXBUF_LOADER_TESTING
gdk_pixbuf_loader_0005_context_free_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS) \
        -DGDK_PIXBUF_ENABLE_BACKEND \
        -DSIXEL_PIXBUF_LOADER_TESTING
gdk_pixbuf_loader_0002_incremental_load_LDADD = \
        $(top_builddir)/src/libsixel.la \
        $(GDK_PIXBUF_LIBS)
gdk_pixbuf_loader_0003_corrupt_data_LDADD = \
        $(top_builddir)/src/libsixel.la \
        $(GDK_PIXBUF_LIBS)
gdk_pixbuf_loader_0004_propagate_error_LDADD = \
        $(top_builddir)/src/libsixel.la \
        $(GDK_PIXBUF_LIBS)
gdk_pixbuf_loader_0005_context_free_LDADD = \
        $(top_builddir)/src/libsixel.la \
        $(GDK_PIXBUF_LIBS)
endif
TEST_EXTENSIONS = .t

HELPER_FILES = common/t/0001_converters_common.t \
       python/lib/common.sh \
       palette/t/kmeans_init_common.sh \
       pipeline/t/pipeline_planner_common.sh \
       mapfile/t/mapfile_common.sh \
       packaging/t/python_wheel_common.sh

ARTIFACT_ROOT = $(abs_builddir)/_artifacts
AM_TESTS_ENVIRONMENT = ARTIFACT_ROOT='$(ARTIFACT_ROOT)'; export ARTIFACT_ROOT;
AM_TESTS_ENVIRONMENT += TOP_SRCDIR='$(abs_top_srcdir)'; export TOP_SRCDIR;
AM_TESTS_ENVIRONMENT += TOP_BUILDDIR='$(abs_top_builddir)'; export TOP_BUILDDIR;
AM_TESTS_ENVIRONMENT += CROSS_COMPILING='@cross_compiling@'; \
       export CROSS_COMPILING;
TESTS_COMMON_CPPFLAGS_VAL = $(AM_CPPFLAGS) \
       -I$(top_builddir) -I$(top_builddir)/include \
       -I$(top_srcdir)/include -I$(top_srcdir)/src \
       -I$(top_srcdir)/tests/filter \
       -I$(top_srcdir)
if WANT_AMALGAMATED_TOOLS
TESTS_COMMON_CFLAGS_VAL = $(AM_CFLAGS) \
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
TESTS_COMMON_CFLAGS_VAL = $(AM_CFLAGS) \
        $(MAYBE_COVERAGE)
endif
TESTS_COMMON_LDFLAGS_VAL = $(AM_LDFLAGS) $(MAYBE_COVERAGE)
if WANT_AMALGAMATED_TOOLS
TESTS_COMMON_LDADD_VAL = $(LIBS) \
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
TESTS_COMMON_LDADD_VAL = $(top_builddir)/src/libsixel.la $(LIBS)
endif

TESTS_AMALGAMATION_CPPFLAGS_COMMON = $(TESTS_COMMON_CPPFLAGS_VAL)
TESTS_AMALGAMATION_CFLAGS_COMMON = $(TESTS_COMMON_CFLAGS_VAL)
TESTS_AMALGAMATION_LDFLAGS_COMMON = $(TESTS_COMMON_LDFLAGS_VAL)
if AMALGAMATION_NEEDS_OBJC
TESTS_AMALGAMATION_CPPFLAGS_COMMON += -x objective-c
endif
if WANT_AMALGAMATED_TOOLS
TESTS_AMALGAMATION_LDADD_COMMON = $(LIBS) \
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
TESTS_AMALGAMATION_LDADD_COMMON = $(top_builddir)/src/libsixel.la $(LIBS)
endif

check_PROGRAMS += $(FILTER_TEST_BINARIES) $(PROBE_TESTS_BINARIES) $(PALETTE_TESTS_BINARIES)

if WANT_AMALGAMATED_TOOLS
nodist_filter_0001_filter_clip_SOURCES = $(AMALGAMATION_SIXEL)
filter_0001_filter_clip_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0001_FILTER_CLIP=1
filter_0001_filter_clip_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
filter_0001_filter_clip_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
filter_0001_filter_clip_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
filter_0001_filter_clip_SOURCES = filter/0001_filter_clip.c
filter_0001_filter_clip_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
filter_0001_filter_clip_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
filter_0001_filter_clip_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
filter_0001_filter_clip_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_0002_filter_sample_SOURCES = $(AMALGAMATION_SIXEL)
filter_0002_filter_sample_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0002_FILTER_SAMPLE=1
filter_0002_filter_sample_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
filter_0002_filter_sample_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
filter_0002_filter_sample_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
filter_0002_filter_sample_SOURCES = filter/0002_filter_sample.c
filter_0002_filter_sample_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
filter_0002_filter_sample_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
filter_0002_filter_sample_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
filter_0002_filter_sample_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_0003_filter_resize_SOURCES = $(AMALGAMATION_SIXEL)
filter_0003_filter_resize_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0003_FILTER_RESIZE=1
filter_0003_filter_resize_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
filter_0003_filter_resize_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
filter_0003_filter_resize_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
filter_0003_filter_resize_SOURCES = filter/0003_filter_resize.c
filter_0003_filter_resize_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
filter_0003_filter_resize_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
filter_0003_filter_resize_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
filter_0003_filter_resize_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_0004_filter_colors_SOURCES = $(AMALGAMATION_SIXEL)
filter_0004_filter_colors_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0004_FILTER_COLORS=1
filter_0004_filter_colors_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
filter_0004_filter_colors_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
filter_0004_filter_colors_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
filter_0004_filter_colors_SOURCES = filter/0004_filter_colors.c
filter_0004_filter_colors_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
filter_0004_filter_colors_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
filter_0004_filter_colors_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
filter_0004_filter_colors_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_0005_filter_lookup_SOURCES = $(AMALGAMATION_SIXEL)
filter_0005_filter_lookup_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0005_FILTER_LOOKUP=1
filter_0005_filter_lookup_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
filter_0005_filter_lookup_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
filter_0005_filter_lookup_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
filter_0005_filter_lookup_SOURCES = filter/0005_filter_lookup.c
filter_0005_filter_lookup_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
filter_0005_filter_lookup_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
filter_0005_filter_lookup_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
filter_0005_filter_lookup_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_0006_filter_final_merge_SOURCES = $(AMALGAMATION_SIXEL)
filter_0006_filter_final_merge_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0006_FILTER_FINAL_MERGE=1
filter_0006_filter_final_merge_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
filter_0006_filter_final_merge_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
filter_0006_filter_final_merge_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
filter_0006_filter_final_merge_SOURCES = filter/0006_filter_final_merge.c
filter_0006_filter_final_merge_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
filter_0006_filter_final_merge_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
filter_0006_filter_final_merge_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
filter_0006_filter_final_merge_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_0007_filter_vpte_SOURCES = $(AMALGAMATION_SIXEL)
filter_0007_filter_vpte_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0007_FILTER_VPTE=1
filter_0007_filter_vpte_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
filter_0007_filter_vpte_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
filter_0007_filter_vpte_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
filter_0007_filter_vpte_SOURCES = filter/0007_filter_vpte.c
filter_0007_filter_vpte_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
filter_0007_filter_vpte_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
filter_0007_filter_vpte_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
filter_0007_filter_vpte_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_0008_filter_load_SOURCES = $(AMALGAMATION_SIXEL)
filter_0008_filter_load_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0008_FILTER_LOAD=1
filter_0008_filter_load_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
filter_0008_filter_load_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
filter_0008_filter_load_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
filter_0008_filter_load_SOURCES = filter/0008_filter_load.c
filter_0008_filter_load_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
filter_0008_filter_load_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
filter_0008_filter_load_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
filter_0008_filter_load_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_0009_filter_dither_SOURCES = $(AMALGAMATION_SIXEL)
filter_0009_filter_dither_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0009_FILTER_DITHER=1
filter_0009_filter_dither_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
filter_0009_filter_dither_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
filter_0009_filter_dither_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
filter_0009_filter_dither_SOURCES = filter/0009_filter_dither.c
filter_0009_filter_dither_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
filter_0009_filter_dither_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
filter_0009_filter_dither_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
filter_0009_filter_dither_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_filter_0010_filter_encode_SOURCES = $(AMALGAMATION_SIXEL)
filter_0010_filter_encode_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0010_FILTER_ENCODE=1
filter_0010_filter_encode_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
filter_0010_filter_encode_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
filter_0010_filter_encode_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
filter_0010_filter_encode_SOURCES = filter/0010_filter_encode.c
filter_0010_filter_encode_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
filter_0010_filter_encode_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
filter_0010_filter_encode_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
filter_0010_filter_encode_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_cli_0029_cli_token_is_known_option_SOURCES = $(AMALGAMATION_SIXEL)
cli_0029_cli_token_is_known_option_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0029_CLI_TOKEN_IS_KNOWN_OPTION=1 \
    -DBUILD_CLI=1
cli_0029_cli_token_is_known_option_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
cli_0029_cli_token_is_known_option_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
cli_0029_cli_token_is_known_option_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
cli_0029_cli_token_is_known_option_SOURCES = \
    cli/0029_cli_token_is_known_option.c \
    $(top_srcdir)/converters/cli.c
cli_0029_cli_token_is_known_option_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
cli_0029_cli_token_is_known_option_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
cli_0029_cli_token_is_known_option_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
cli_0029_cli_token_is_known_option_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_cli_0030_cli_option_requires_argument_SOURCES = $(AMALGAMATION_SIXEL)
cli_0030_cli_option_requires_argument_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0030_CLI_OPTION_REQUIRES_ARGUMENT=1 \
    -DBUILD_CLI=1
cli_0030_cli_option_requires_argument_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
cli_0030_cli_option_requires_argument_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
cli_0030_cli_option_requires_argument_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
cli_0030_cli_option_requires_argument_SOURCES = \
    cli/0030_cli_option_requires_argument.c \
    $(top_srcdir)/converters/cli.c
cli_0030_cli_option_requires_argument_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
cli_0030_cli_option_requires_argument_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
cli_0030_cli_option_requires_argument_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
cli_0030_cli_option_requires_argument_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_cli_0031_cli_guard_missing_argument_SOURCES = $(AMALGAMATION_SIXEL)
cli_0031_cli_guard_missing_argument_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0031_CLI_GUARD_MISSING_ARGUMENT=1 \
    -DBUILD_CLI=1
cli_0031_cli_guard_missing_argument_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
cli_0031_cli_guard_missing_argument_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
cli_0031_cli_guard_missing_argument_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
cli_0031_cli_guard_missing_argument_SOURCES = \
    cli/0031_cli_guard_missing_argument.c \
    $(top_srcdir)/converters/cli.c
cli_0031_cli_guard_missing_argument_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
cli_0031_cli_guard_missing_argument_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
cli_0031_cli_guard_missing_argument_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
cli_0031_cli_guard_missing_argument_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_probe_0001_probe_parse_SOURCES = $(AMALGAMATION_SIXEL)
probe_0001_probe_parse_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0001_PROBE_PARSE=1
probe_0001_probe_parse_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
probe_0001_probe_parse_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
probe_0001_probe_parse_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
probe_0001_probe_parse_SOURCES = probe/0001_probe_parse.c
probe_0001_probe_parse_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
probe_0001_probe_parse_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
probe_0001_probe_parse_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
probe_0001_probe_parse_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_palette_0001_kmeans_init_SOURCES = $(AMALGAMATION_SIXEL)
palette_0001_kmeans_init_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0001_KMEANS_INIT=1
palette_0001_kmeans_init_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
palette_0001_kmeans_init_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
palette_0001_kmeans_init_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
palette_0001_kmeans_init_SOURCES = palette/0001_kmeans_init.c
palette_0001_kmeans_init_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
palette_0001_kmeans_init_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
palette_0001_kmeans_init_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
palette_0001_kmeans_init_LDADD = $(TESTS_COMMON_LDADD_VAL)
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
             loader/pngsuite_common.sh \
             $(BASELINE_JSON) $(INPUT_FORMAT_FILES) \
             $(RESOLUTION_IMAGES) $(CORRUPTED_INPUTS) $(ISSUE_INPUTS) \
             filter/0001_filter_clip.c filter/0002_filter_sample.c \
             filter/0003_filter_resize.c filter/0004_filter_colors.c \
             filter/0005_filter_lookup.c filter/0006_filter_final_merge.c \
             filter/0007_filter_vpte.c filter/0008_filter_load.c \
             filter/0009_filter_dither.c filter/0010_filter_encode.c \
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
