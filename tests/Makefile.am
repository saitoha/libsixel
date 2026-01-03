if COND_GCOV
MAYBE_COVERAGE = --coverage
endif
if CFLAGS_HAVE_COVERAGE
MAYBE_COVERAGE = --coverage
endif
if USING_CL
LIBTOOL = $(SHELL) $(top_srcdir)/build-aux/libtool-normalize-exe.sh -- \
    $(SHELL) $(top_builddir)/libtool
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
CLI_TESTS = cli-core/t/0001_help.t \
    cli-core/t/0002_version.t \
    cli-core/t/0003_stdin.t \
    cli-core/t/0004_basic_missing_input.t \
    cli-core/t/0005_basic_help_option.t \
    cli-core/t/0006_basic_version_option.t \
    cli-core/t/0007_basic_stdin_snake.t \
    cli-core/t/0008_basic_stdin_map8.t \
    cli-core/t/0009_basic_stdin_stdout_map64.t \
    cli-core/t/0010_basic_file_arguments_snake.t \
    cli-core/t/0011_basic_direct_output.t \
    cli-core/t/0012_basic_direct_with_dequantize_conflict.t \
    cli-core/t/0013_basic_png_prefix_path.t \
    cli-core/t/0014_basic_png_stdout.t \
    cli-core/t/0015_basic_png_prefix_missing_target.t \
    cli-core/t/0016_basic_dequantize_unique_prefix.t \
    cli-core/t/0017_basic_parallel_indexed.t \
    cli-core/t/0018_basic_parallel_direct.t \
    cli-core/t/0019_basic_parallel_cli_override.t \
    cli-core/t/0020_basic_invalid_threads_token.t \
    cli-option-invalid/t/0001_invalid_option_arguments.t \
    cli-option-invalid/t/0002_invalid_option_arguments_invalid_width.t \
    cli-option-invalid/t/0003_invalid_option_arguments_invalid_height.t \
    cli-option-invalid/t/0004_invalid_option_arguments_invalid_format.t \
    cli-option-invalid/t/0005_invalid_option_arguments_invalid_quality.t \
    cli-option-invalid/t/0006_invalid_option_arguments_invalid_layout.t \
    cli-option-invalid/t/0007_invalid_option_arguments_invalid_bpp.t \
    cli-option-invalid/t/0008_invalid_option_arguments_invalid_encoder_tweak.t \
    cli-option-invalid/t/0009_invalid_option_arguments_invalid_background_colour.t \
    cli-option-invalid/t/0010_invalid_option_arguments_missing_background_component.t \
    cli-option-invalid/t/0011_invalid_option_arguments_overlong_background.t \
    cli-option-invalid/t/0012_invalid_option_arguments_malformed_hex_background.t \
    cli-option-invalid/t/0013_invalid_option_arguments_unknown_named_colour.t \
    cli-option-invalid/t/0014_invalid_option_arguments_incomplete_rgb_background.t \
    cli-option-invalid/t/0015_invalid_option_arguments_unsupported_legacy_width.t \
    cli-option-invalid/t/0016_invalid_option_arguments_missing_palette_file.t \
    cli-option-invalid/t/0017_invalid_option_arguments_invalid_colour_space_index.t \
    cli-option-invalid/t/0018_invalid_option_arguments_unknown_dither.t \
    cli-option-invalid/t/0019_invalid_option_arguments_unknown_resize_filter.t \
    cli-option-invalid/t/0020_invalid_option_arguments_unknown_scaling_mode.t \
    cli-option-invalid/t/0021_invalid_option_arguments_unknown_tone_adjustment.t \
    cli-option-invalid/t/0022_invalid_option_combinations.t \
    cli-option-invalid/t/0023_invalid_option_combinations_palette_and_encode.t \
    cli-option-invalid/t/0024_invalid_option_combinations_inspect_with_palette.t \
    cli-option-invalid/t/0025_invalid_option_combinations_palette_with_terminal_preset.t \
    cli-option-invalid/t/0026_invalid_option_combinations_eight_bit_with_palette_dump.t \
    cli-option-matching/t/0001_option_matching_prefix_unique.t \
    cli-option-matching/t/0002_option_matching_prefix_ambiguous.t \
    cli-option-matching/t/0003_option_matching_diffusion_correction.t \
    cli-option-matching/t/0004_option_matching_distance1_multi.t \
    cli-option-matching/t/0005_option_matching_distance2.t \
    cli-option-matching/t/0006_option_matching_distance3.t \
    cli-argument-shift/t/0001_invalid_positional.t \
    cli-argument-shift/t/0002_argument_shift_missing_mapfile.t \
    cli-argument-shift/t/0003_argument_shift_outfile_named_like_option.t \
    cli-argument-shift/t/0004_argument_shift_assessment_named_like_option.t \
    cli-argument-shift/t/0005_cli_token_is_known_option.t \
    cli-argument-shift/t/0006_cli_option_requires_argument.t \
    cli-argument-shift/t/0007_cli_guard_missing_argument.t \
    palette/t/0008_select_color_ambiguous_prefix_rejected.t \
    palette/t/0009_select_color_unique_prefix_accepted.t \
    palette/t/0010_explicit_dimensions_with_palette.t \
    palette/t/0011_fast_encoder_external_palette.t \
    palette/t/0012_palette16_conversion.t \
    palette/t/0013_xterm_palette_override_repetition.t \
    palette/t/0014_tiff_palette_filters.t \
    palette/t/0015_grayscale_png_with_palette.t \
    palette/t/0016_apply_grayscale_palette_to_colour_png.t \
    palette/t/0017_stacked_palette_files.t \
    palette/t/0018_xterm16_palette_preset.t \
    palette/t/0019_jpeg_palette_welsh.t \
    palette/t/0020_ppm_palette16_conversion.t \
    palette/t/0021_lanczos2_palette_limit.t \
    palette/t/0022_lanczos3_two_colour_palette.t \
    palette/t/0023_lanczos4_palette_dump.t \
    palette/t/0024_interlaced_palette_dump.t \
    dither/t/0001_roundtrip_dither_gamma.t \
    dither/t/0002_inspection_with_diffusion.t \
    dither/t/0003_ascii_ppm_inspection_diffusion.t \
    dither/t/0004_ascii_pbm_atkinson_inspection.t \
    dither/t/0005_cropping_with_x_dither.t \
    dither/t/0006_cropping_with_alternate_dither.t \
    dither/t/0007_sixel_inspection_alternate_dither.t \
    dither/t/0008_sixel_inspection_x_dither.t \
    dither/t/0009_lanczos2_without_diffusion.t \
    geometry/t/0001_scaling_histogram_background.t \
    geometry/t/0002_dcs_crc_width_scaling.t \
    geometry/t/0003_dcs_crc_height_scaling.t \
    geometry/t/0004_dcs_crc_height_scaling_auto_width.t \
    geometry/t/0005_dcs_crc_height_auto_width_absolute.t \
    geometry/t/0006_dcs_crc_height_absolute_width_percent.t \
    geometry/t/0007_tga_scaling.t \
    geometry/t/0008_indexed_png_scaling.t \
    geometry/t/0009_sixel_resize_palette_limit.t \
    geometry/t/0010_sixel_cropping_large_offsets.t \
    geometry/t/0011_ppm_inspection_aggressive_scaling.t \
    geometry/t/0012_resize_precision_mode_run.t \
    geometry/t/0013_resize_precision_mode_scaler_input.t \
    geometry/t/0014_resize_precision_mode_post_colorspace.t \
    format/t/0001_prefixed_png_output_default.t \
    format/t/0002_prefixed_png_output_explicit_path.t \
    format/t/0003_filename_driven_png_header.t \
    format/t/0004_gif_filters.t \
    format/t/0005_pgm_encode_flag.t \
    format/t/0006_gray1_output.t \
    format/t/0007_gray2_output.t \
    format/t/0008_gray4_output.t \
    format/t/0009_gray8_output.t \
    format/t/0010_png_inspection_color_space.t \
    format/t/0011_ascii_pgm_auto_encoder_inspection.t \
    format/t/0012_grayscale_png_conversion.t \
    format/t/0013_grayscale_png_inspection.t \
    format/t/0014_grayscale_jpeg_inspection.t \
    format/t/0015_bmp_inspection_with_filters.t \
    output/t/0001_vt340_colour_sequences.t \
    output/t/0002_vt340_monochrome_sequences.t \
    output/t/0003_direct_sixel_encode.t \
    output/t/0004_sixel_metadata_inspection.t \
    output/t/0005_monochrome_frame_trailer.t \
    output/t/0006_two_pass_sixel_conversion.t \
    cli-options/t/0001_long_option_forms.t \
    decoder/t/0001_dcs_arguments_j0.t \
    decoder/t/0002_dcs_arguments_j1.t \
    decoder/t/0003_dcs_arguments_j2.t \
    decoder/t/0004_tab_separated_colour_introducers.t \
    decoder/t/0005_oversized_dcs_geometry.t \
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
    regression/t/0005_lsqa_format_grayscale.t \
    regression/t/0006_lsqa_format_palette.t \
    regression/t/0007_lsqa_format_rgb.t \
    regression/t/0008_lsqa_format_rgba.t \
    regression/t/0009_lsqa_resolution_hd_landscape.t \
    regression/t/0010_lsqa_resolution_panorama_tall.t \
    regression/t/0011_lsqa_resolution_panorama_wide.t \
    regression/t/0012_lsqa_resolution_quarter_square.t \
    regression/t/0013_lsqa_resolution_svga_landscape.t \
    regression/t/0014_lsqa_resolution_tiny_square.t \
    regression/t/0015_lsqa_repeat_palette_variance.t \
    regression/t/0016_lsqa_corrupted_metadata_noise.t \
    regression/t/0017_lsqa_corrupted_truncated_png.t \
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

TESTS_COMMON_TESTS = \
       loader/0008_loader_coregraphics_pixelformat \
       loader/0009_loader_wic_pixelformat \
       loader/0010_loader_gdk_pixbuf_pixelformat \
       loader/0011_loader_gd_pixelformat \
       loader/0012_loader_libpng_pixelformat \
       loader/0013_loader_libjpeg_pixelformat \
       loader/0014_loader_builtin_pixelformat \
       loader/0015_loader_quicklook_pixelformat \
       loader/0016_loader_gnome_thumbnailer_pixelformat

TESTS = $(CLI_TESTS) $(PROBE_TESTS) $(PALETTE_TESTS) $(PYTHON_TESTS) \
       $(FILTER_TESTS) $(TESTS_COMMON_TESTS)
COPY_TESTS = $(CLI_TESTS) $(PROBE_TESTS) $(PALETTE_TESTS) $(PYTHON_TESTS) \
       $(FILTER_TESTS)
check_PROGRAMS = \
    cli/0029_cli_token_is_known_option \
    cli/0030_cli_option_requires_argument \
    cli/0031_cli_guard_missing_argument

if ENABLE_GDK_PIXBUF_LOADER
TESTS += gdk-pixbuf-loader/0001_gdk_pixbuf_loader \
    gdk-pixbuf-loader/0002_incremental_load \
    gdk-pixbuf-loader/0003_corrupt_data \
    gdk-pixbuf-loader/0004_propagate_error \
    gdk-pixbuf-loader/0005_context_free \
    gdk-pixbuf-loader/0006_loader_module \
    gdk-pixbuf-loader/0007_loader_plugin_integration
check_PROGRAMS += gdk-pixbuf-loader/0001_gdk_pixbuf_loader \
    gdk-pixbuf-loader/0002_incremental_load \
    gdk-pixbuf-loader/0003_corrupt_data \
    gdk-pixbuf-loader/0004_propagate_error \
    gdk-pixbuf-loader/0005_context_free \
    gdk-pixbuf-loader/0006_loader_module \
    gdk-pixbuf-loader/0007_loader_plugin_integration
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
gdk_pixbuf_loader_0006_loader_module_SOURCES = \
    gdk-pixbuf-loader/0006_loader_module.c
gdk_pixbuf_loader_0007_loader_plugin_integration_SOURCES = \
    gdk-pixbuf-loader/0007_loader_plugin_integration.c
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
gdk_pixbuf_loader_0006_loader_module_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS)
gdk_pixbuf_loader_0007_loader_plugin_integration_CPPFLAGS = $(AM_CPPFLAGS) \
        -I$(top_builddir)/include \
        -I$(top_srcdir)/include \
        -I$(top_srcdir)/src \
        $(GDK_PIXBUF_CFLAGS)
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
gdk_pixbuf_loader_0006_loader_module_LDADD = \
        $(top_builddir)/src/libsixel.la \
        $(GDK_PIXBUF_LIBS)
gdk_pixbuf_loader_0007_loader_plugin_integration_LDADD = \
        $(top_builddir)/src/libsixel.la \
        $(GDK_PIXBUF_LIBS)
endif
TEST_EXTENSIONS = .t

HELPER_FILES = common/t/0001_converters_common.t \
       conversion/common.sh \
       cli-core/t/cli_core_common.sh \
       python/lib/common.sh \
       palette/t/kmeans_init_common.sh \
       pipeline/t/pipeline_planner_common.sh \
       mapfile/t/mapfile_common.sh \
       packaging/t/python_wheel_common.sh \
       regression/lsqa_common.sh

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

check_PROGRAMS += \
    $(FILTER_TEST_BINARIES) \
    $(PROBE_TESTS_BINARIES) \
    $(PALETTE_TESTS_BINARIES) \
    $(TESTS_COMMON_TESTS)

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

if WANT_AMALGAMATED_TOOLS
nodist_loader_0008_loader_coregraphics_pixelformat_SOURCES = $(AMALGAMATION_SIXEL)
loader_0008_loader_coregraphics_pixelformat_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0008_LOADER_COREGRAPHICS_PIXELFORMAT=1
loader_0008_loader_coregraphics_pixelformat_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
loader_0008_loader_coregraphics_pixelformat_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
loader_0008_loader_coregraphics_pixelformat_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
loader_0008_loader_coregraphics_pixelformat_SOURCES = \
    loader/0008_loader_coregraphics_pixelformat.c
loader_0008_loader_coregraphics_pixelformat_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
loader_0008_loader_coregraphics_pixelformat_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
loader_0008_loader_coregraphics_pixelformat_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
loader_0008_loader_coregraphics_pixelformat_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_loader_0009_loader_wic_pixelformat_SOURCES = $(AMALGAMATION_SIXEL)
loader_0009_loader_wic_pixelformat_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0009_LOADER_WIC_PIXELFORMAT=1
loader_0009_loader_wic_pixelformat_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
loader_0009_loader_wic_pixelformat_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
loader_0009_loader_wic_pixelformat_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
loader_0009_loader_wic_pixelformat_SOURCES = \
    loader/0009_loader_wic_pixelformat.c \
    loader/pixelformat_test_common.h
loader_0009_loader_wic_pixelformat_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
loader_0009_loader_wic_pixelformat_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
loader_0009_loader_wic_pixelformat_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
loader_0009_loader_wic_pixelformat_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_loader_0010_loader_gdk_pixbuf_pixelformat_SOURCES = $(AMALGAMATION_SIXEL)
loader_0010_loader_gdk_pixbuf_pixelformat_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0010_LOADER_GDK_PIXBUF_PIXELFORMAT=1
loader_0010_loader_gdk_pixbuf_pixelformat_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
loader_0010_loader_gdk_pixbuf_pixelformat_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
loader_0010_loader_gdk_pixbuf_pixelformat_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
loader_0010_loader_gdk_pixbuf_pixelformat_SOURCES = \
    loader/0010_loader_gdk_pixbuf_pixelformat.c \
    loader/pixelformat_test_common.h
loader_0010_loader_gdk_pixbuf_pixelformat_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
loader_0010_loader_gdk_pixbuf_pixelformat_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
loader_0010_loader_gdk_pixbuf_pixelformat_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
loader_0010_loader_gdk_pixbuf_pixelformat_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_loader_0011_loader_gd_pixelformat_SOURCES = $(AMALGAMATION_SIXEL)
loader_0011_loader_gd_pixelformat_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0011_LOADER_GD_PIXELFORMAT=1
loader_0011_loader_gd_pixelformat_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
loader_0011_loader_gd_pixelformat_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
loader_0011_loader_gd_pixelformat_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
loader_0011_loader_gd_pixelformat_SOURCES = \
    loader/0011_loader_gd_pixelformat.c \
    loader/pixelformat_test_common.h
loader_0011_loader_gd_pixelformat_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
loader_0011_loader_gd_pixelformat_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
loader_0011_loader_gd_pixelformat_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
loader_0011_loader_gd_pixelformat_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_loader_0012_loader_libpng_pixelformat_SOURCES = $(AMALGAMATION_SIXEL)
loader_0012_loader_libpng_pixelformat_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0012_LOADER_LIBPNG_PIXELFORMAT=1
loader_0012_loader_libpng_pixelformat_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
loader_0012_loader_libpng_pixelformat_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
loader_0012_loader_libpng_pixelformat_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
loader_0012_loader_libpng_pixelformat_SOURCES = \
    loader/0012_loader_libpng_pixelformat.c \
    loader/pixelformat_test_common.h
loader_0012_loader_libpng_pixelformat_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
loader_0012_loader_libpng_pixelformat_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
loader_0012_loader_libpng_pixelformat_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
loader_0012_loader_libpng_pixelformat_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_loader_0013_loader_libjpeg_pixelformat_SOURCES = $(AMALGAMATION_SIXEL)
loader_0013_loader_libjpeg_pixelformat_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0013_LOADER_LIBJPEG_PIXELFORMAT=1
loader_0013_loader_libjpeg_pixelformat_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
loader_0013_loader_libjpeg_pixelformat_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
loader_0013_loader_libjpeg_pixelformat_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
loader_0013_loader_libjpeg_pixelformat_SOURCES = \
    loader/0013_loader_libjpeg_pixelformat.c \
    loader/pixelformat_test_common.h
loader_0013_loader_libjpeg_pixelformat_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
loader_0013_loader_libjpeg_pixelformat_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
loader_0013_loader_libjpeg_pixelformat_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
loader_0013_loader_libjpeg_pixelformat_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_loader_0014_loader_builtin_pixelformat_SOURCES = $(AMALGAMATION_SIXEL)
loader_0014_loader_builtin_pixelformat_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0014_LOADER_BUILTIN_PIXELFORMAT=1
loader_0014_loader_builtin_pixelformat_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
loader_0014_loader_builtin_pixelformat_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
loader_0014_loader_builtin_pixelformat_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
loader_0014_loader_builtin_pixelformat_SOURCES = \
    loader/0014_loader_builtin_pixelformat.c \
    loader/pixelformat_test_common.h
loader_0014_loader_builtin_pixelformat_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
loader_0014_loader_builtin_pixelformat_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
loader_0014_loader_builtin_pixelformat_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
loader_0014_loader_builtin_pixelformat_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_loader_0015_loader_quicklook_pixelformat_SOURCES = $(AMALGAMATION_SIXEL)
loader_0015_loader_quicklook_pixelformat_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0015_LOADER_QUICKLOOK_PIXELFORMAT=1
loader_0015_loader_quicklook_pixelformat_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
loader_0015_loader_quicklook_pixelformat_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
loader_0015_loader_quicklook_pixelformat_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
loader_0015_loader_quicklook_pixelformat_SOURCES = \
    loader/0015_loader_quicklook_pixelformat.c \
    loader/pixelformat_test_common.h
loader_0015_loader_quicklook_pixelformat_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
loader_0015_loader_quicklook_pixelformat_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
loader_0015_loader_quicklook_pixelformat_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
loader_0015_loader_quicklook_pixelformat_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif

if WANT_AMALGAMATED_TOOLS
nodist_loader_0016_loader_gnome_thumbnailer_pixelformat_SOURCES = $(AMALGAMATION_SIXEL)
loader_0016_loader_gnome_thumbnailer_pixelformat_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    -DBUILD_0016_LOADER_GNOME_THUMBNAILER_PIXELFORMAT=1
loader_0016_loader_gnome_thumbnailer_pixelformat_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
loader_0016_loader_gnome_thumbnailer_pixelformat_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
loader_0016_loader_gnome_thumbnailer_pixelformat_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
loader_0016_loader_gnome_thumbnailer_pixelformat_SOURCES = \
    loader/0016_loader_gnome_thumbnailer_pixelformat.c \
    loader/pixelformat_test_common.h
loader_0016_loader_gnome_thumbnailer_pixelformat_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL)
loader_0016_loader_gnome_thumbnailer_pixelformat_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
loader_0016_loader_gnome_thumbnailer_pixelformat_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
loader_0016_loader_gnome_thumbnailer_pixelformat_LDADD = $(TESTS_COMMON_LDADD_VAL)
endif


LOG_DRIVER = $(T_LOG_DRIVER)
LOG_DRIVER_FLAGS = $(AM_TEST_LOG_DRIVER_FLAGS)
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_builddir)/build-aux/lso-tap-driver.sh
AM_TEST_LOG_DRIVER_FLAGS = --comments
AM_TEST_TIMEOUT = 300
AM_LOG_DRIVER_FLAGS = --timeout=$(AM_TEST_TIMEOUT)
# Leave AM_LOG_FLAGS empty so Automake does not append timeout parameters to
# LOG_COMPILE; the drivers consume timeout hints themselves.
AM_LOG_FLAGS =

BASELINE_JSON = data/baseline/grayscale.json \
       data/baseline/hd_landscape.json \
       data/baseline/palette.json \
       data/baseline/panorama_tall.json \
       data/baseline/panorama_wide.json \
       data/baseline/quarter_square.json \
       data/baseline/rgb.json \
       data/baseline/rgba.json \
       data/baseline/svga_landscape.json \
       data/baseline/tiny_square.json

INPUT_FORMAT_FILES = data/inputs/formats/grayscale.jpg \
       data/inputs/formats/palette.png \
       data/inputs/formats/rgb.png \
       data/inputs/formats/rgba.png

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
