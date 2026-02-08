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
TESTS = \
    cli/aborttrace/0001_img2sixel_aborttrace.t \
    codec/decoder/animation/0001_disable_update_mode.t \
    codec/decoder/animation/0002_static_frame_render.t \
    codec/decoder/animation/0003_update_and_static_frame.t \
    codec/decoder/animation/0004_split_sequence_atkinson.t \
    cli/argument-shift/0001_invalid_positional.t \
    cli/argument-shift/0002_argument_shift_missing_mapfile.t \
    cli/argument-shift/0003_argument_shift_outfile_named_like_option.t \
    cli/argument-shift/0005_cli_token_is_known_option.t \
    cli/argument-shift/0006_cli_option_requires_argument.t \
    cli/argument-shift/0007_cli_guard_missing_argument.t \
    cli/core/0001_help.t \
    cli/core/0002_version.t \
    cli/core/0003_stdin.t \
    cli/core/0004_basic_missing_input.t \
    cli/core/0005_basic_help_option.t \
    cli/core/0006_basic_version_option.t \
    cli/core/0007_basic_stdin_snake.t \
    cli/core/0008_basic_stdin_map8.t \
    cli/core/0009_basic_stdin_stdout_map64.t \
    cli/core/0010_basic_file_arguments_snake.t \
    cli/core/0011_basic_direct_output.t \
    cli/core/0012_basic_direct_with_dequantize_conflict.t \
    cli/core/0013_basic_png_prefix_path.t \
    cli/core/0014_basic_png_stdout.t \
    cli/core/0015_basic_png_prefix_missing_target.t \
    cli/core/0016_basic_dequantize_unique_prefix.t \
    cli/core/0017_basic_parallel_indexed.t \
    cli/core/0018_basic_parallel_direct.t \
    cli/core/0019_basic_parallel_cli_override.t \
    cli/core/0020_basic_invalid_threads_token.t \
    cli/options/invalid/0001_invalid_option_arguments_invalid_width.t \
    cli/options/invalid/0002_invalid_option_arguments_invalid_height.t \
    cli/options/invalid/0003_invalid_option_arguments_invalid_findtype.t \
    cli/options/invalid/0004_invalid_option_arguments_invalid_quality.t \
    cli/options/invalid/0005_invalid_option_arguments_invalid_loop_mode.t \
    cli/options/invalid/0006_invalid_option_arguments_invalid_builtin_palette.t \
    cli/options/invalid/0007_invalid_option_arguments_invalid_encode_policy.t \
    cli/options/invalid/0008_invalid_option_arguments_invalid_background_colour.t \
    cli/options/invalid/0009_invalid_option_arguments_missing_background_component.t \
    cli/options/invalid/0010_invalid_option_arguments_overlong_background.t \
    cli/options/invalid/0011_invalid_option_arguments_malformed_hex_background.t \
    cli/options/invalid/0012_invalid_option_arguments_unknown_named_colour.t \
    cli/options/invalid/0013_invalid_option_arguments_incomplete_rgb_background.t \
    cli/options/invalid/0014_invalid_option_arguments_unsupported_legacy_width.t \
    cli/options/invalid/0015_invalid_option_arguments_missing_palette_file.t \
    cli/options/invalid/0016_invalid_option_arguments_invalid_complexion_score.t \
    cli/options/invalid/0017_invalid_option_arguments_unknown_dither.t \
    cli/options/invalid/0018_invalid_option_arguments_unknown_resampling_option.t \
    cli/options/invalid/0019_invalid_option_arguments_unknown_select_type_mode.t \
    cli/options/invalid/0020_invalid_option_arguments_unknown_palette_type.t \
    cli/options/invalid/0021_invalid_option_combinations_palette_and_encode.t \
    cli/options/invalid/0022_invalid_option_combinations_high_color_with_palette.t \
    cli/options/invalid/0023_invalid_option_combinations_palette_with_terminal_preset.t \
    cli/options/invalid/0024_invalid_option_combinations_eight_bit_with_pipe_mode.t \
    cli/options/matching/0001_option_matching_prefix_unique.t \
    cli/options/matching/0002_option_matching_prefix_ambiguous.t \
    cli/options/matching/0003_option_matching_diffusion_correction.t \
    cli/options/matching/0004_option_matching_distance1_multi.t \
    cli/options/matching/0005_option_matching_distance2.t \
    cli/options/matching/0006_option_matching_distance3.t \
    cli/options/general/0001_long_option_forms.t \
    cli/options/general/0002_ormode_option_runs.t \
    cli/options/general/0003_gri_limit_lsqa.t \
    io/clipboard/0001_clipboard_basic.t \
    cli/completion/0021_completion_show_bash.t \
    cli/completion/0022_completion_show_zsh.t \
    cli/completion/0023_completion_install_bash.t \
    cli/completion/0024_completion_install_zsh.t \
    cli/completion/0025_completion_uninstall_bash.t \
    cli/completion/0026_completion_uninstall_zsh.t \
    codec/decoder/core/0004_tab_separated_colour_introducers.t \
    codec/decoder/core/0005_oversized_dcs_geometry.t \
    processing/dither/general/0001_roundtrip_dither_gamma.t \
    processing/dither/general/0002_inspection_with_diffusion.t \
    processing/dither/general/0003_ascii_ppm_inspection_diffusion.t \
    processing/dither/general/0004_ascii_pbm_atkinson_inspection.t \
    processing/dither/x-dither/0005_cropping_with_x_dither.t \
    processing/dither/general/0006_cropping_with_alternate_dither.t \
    processing/dither/general/0007_sixel_inspection_alternate_dither.t \
    processing/dither/x-dither/0008_sixel_inspection_x_dither.t \
    processing/dither/general/0009_lanczos2_without_diffusion.t \
    processing/dither/general/0010_inspection_high_color_sierra1.t \
    processing/dither/general/0011_inspection_high_color_sierra2.t \
    processing/dither/general/0012_inspection_high_color_sierra3.t \
    processing/dither/general/0013_inspection_high_color_rgba_input.t \
    processing/dither/positional/0010_positional_8bit_dither.t \
    processing/dither/positional/0011_positional_float32_dither.t \
    processing/dither/varcoeff/0012_varcoeff_8bit_dither.t \
    processing/dither/varcoeff/0013_varcoeff_float32_dither.t \
    processing/dither/varcoeff/0014_varcoeff_8bit_serpentine_lsqa.t \
    processing/dither/varcoeff/0015_varcoeff_8bit_carry_lsqa.t \
    processing/dither/varcoeff/0016_varcoeff_8bit_carry_serpentine_lsqa.t \
    processing/dither/varcoeff/0017_varcoeff_float32_carry_lsqa.t \
    processing/dither/varcoeff/0018_varcoeff_float32_carry_serpentine_lsqa.t \
    processing/dither/varcoeff/0019_varcoeff_float32_oklab_lsqa.t \
    processing/dither/x-dither/0020_positional_8bit_x_dither_lsqa.t \
    processing/dither/positional/0021_positional_8bit_no_diffuse_lsqa.t \
    processing/dither/a-dither/0022_positional_float32_a_dither_lsqa.t \
    processing/dither/positional/0023_positional_float32_no_diffuse_lsqa.t \
    processing/dither/fixed/0024_fixed_8bit_fs_carry_lsqa.t \
    processing/dither/fixed/0025_fixed_8bit_fs_carry_serpentine_lsqa.t \
    processing/dither/fixed/0026_fixed_8bit_atkinson_carry_lsqa.t \
    processing/dither/fixed/0027_fixed_8bit_jajuni_carry_lsqa.t \
    processing/dither/fixed/0028_fixed_8bit_stucki_carry_lsqa.t \
    processing/dither/fixed/0029_fixed_8bit_burkes_carry_lsqa.t \
    processing/dither/fixed/0030_fixed_8bit_sierra1_lsqa.t \
    processing/dither/fixed/0031_fixed_8bit_sierra1_carry_lsqa.t \
    processing/dither/fixed/0032_fixed_8bit_sierra2_lsqa.t \
    processing/dither/fixed/0033_fixed_8bit_sierra2_carry_lsqa.t \
    processing/dither/fixed/0034_fixed_8bit_sierra3_lsqa.t \
    processing/dither/fixed/0035_fixed_8bit_sierra3_carry_lsqa.t \
    processing/dither/fixed/0036_fixed_float32_fs_lsqa.t \
    processing/dither/fixed/0037_fixed_float32_fs_serpentine_lsqa.t \
    processing/dither/fixed/0038_fixed_float32_atkinson_lsqa.t \
    processing/dither/fixed/0039_fixed_float32_jajuni_lsqa.t \
    processing/dither/fixed/0040_fixed_float32_stucki_lsqa.t \
    processing/dither/fixed/0041_fixed_float32_burkes_lsqa.t \
	processing/dither/fixed/0042_fixed_float32_sierra1_lsqa.t \
	processing/dither/fixed/0043_fixed_float32_sierra2_lsqa.t \
	processing/dither/fixed/0044_fixed_float32_sierra3_lsqa.t \
	processing/dither/fixed/0045_fixed_float32_lookup_none_lsqa.t \
    processing/dither/bluenoise/0045_positional_8bit_bluenoise_gamma_lsqa.t \
    processing/dither/bluenoise/0046_positional_float32_bluenoise_gamma_lsqa.t \
    processing/dither/bluenoise/0047_positional_float32_bluenoise_oklab_lsqa.t \
    processing/dither/bluenoise/0048_positional_float32_bluenoise_linear_lsqa.t \
    processing/dither/bluenoise/0049_positional_float32_bluenoise_cielab_lsqa.t \
    processing/dither/bluenoise/0050_positional_float32_bluenoise_din99d_lsqa.t \
    processing/dither/a-dither/0051_positional_8bit_a_dither_gamma_lsqa.t \
    processing/dither/a-dither/0052_positional_float32_a_dither_gamma_lsqa.t \
    processing/dither/a-dither/0053_positional_float32_a_dither_oklab_lsqa.t \
    processing/dither/a-dither/0054_positional_float32_a_dither_linear_lsqa.t \
    processing/dither/a-dither/0055_positional_float32_a_dither_cielab_lsqa.t \
    processing/dither/a-dither/0056_positional_float32_a_dither_din99d_lsqa.t \
    processing/dither/x-dither/0057_positional_8bit_x_dither_gamma_lsqa.t \
    processing/dither/x-dither/0058_positional_float32_x_dither_gamma_lsqa.t \
    processing/dither/x-dither/0059_positional_float32_x_dither_oklab_lsqa.t \
    processing/dither/x-dither/0060_positional_float32_x_dither_linear_lsqa.t \
    processing/dither/x-dither/0061_positional_float32_x_dither_cielab_lsqa.t \
    processing/dither/x-dither/0062_positional_float32_x_dither_din99d_lsqa.t \
    docs/consistency/0001_help_vs_man.t \
    docs/consistency/0002_man_vs_bash_completion.t \
    docs/consistency/0003_bash_vs_zsh_completion.t \
    docs/consistency/0004_envvars_vs_help.t \
    processing/filter/0001_filter_clip.t \
    processing/filter/0002_filter_sample.t \
    processing/filter/0003_filter_resize.t \
    processing/filter/0004_filter_colors.t \
    processing/filter/0005_filter_lookup.t \
    processing/filter/0006_filter_final_merge.t \
    processing/filter/0007_filter_vpte.t \
    processing/filter/0008_filter_load.t \
    processing/filter/0009_filter_dither.t \
    processing/filter/0010_filter_encode.t \
    codec/format/0001_prefixed_png_output_default.t \
    codec/format/0002_prefixed_png_output_explicit_path.t \
    codec/format/0003_filename_driven_png_header.t \
    codec/format/0004_gif_filters.t \
    codec/format/0005_pgm_encode_flag.t \
    codec/format/0006_gray1_output.t \
    codec/format/0007_gray2_output.t \
    codec/format/0008_gray4_output.t \
    codec/format/0009_gray8_output.t \
    codec/format/0010_png_inspection_color_space.t \
    codec/format/0011_ascii_pgm_auto_encoder_inspection.t \
    codec/format/0012_grayscale_png_conversion.t \
    codec/format/0013_grayscale_png_inspection.t \
    codec/format/0014_grayscale_jpeg_inspection.t \
    codec/format/0015_bmp_inspection_with_filters.t \
    extension/gdk-pixbuf-loader/0001_gdk_pixbuf_loader.t \
    extension/gdk-pixbuf-loader/0002_incremental_load.t \
    extension/gdk-pixbuf-loader/0003_corrupt_data.t \
    extension/gdk-pixbuf-loader/0004_propagate_error.t \
    extension/gdk-pixbuf-loader/0005_context_free.t \
    extension/gdk-pixbuf-loader/0006_loader_module.t \
    extension/gdk-pixbuf-loader/0007_loader_plugin_integration.t \
    processing/geometry/0001_scaling_histogram_background.t \
    processing/geometry/0002_dcs_crc_width_scaling.t \
    processing/geometry/0003_dcs_crc_height_scaling.t \
    processing/geometry/0004_dcs_crc_height_scaling_auto_width.t \
    processing/geometry/0005_dcs_crc_height_auto_width_absolute.t \
    processing/geometry/0006_dcs_crc_height_absolute_width_percent.t \
    processing/geometry/0007_tga_scaling.t \
    processing/geometry/0008_indexed_png_scaling.t \
    processing/geometry/0009_sixel_resize_palette_limit.t \
    processing/geometry/0010_sixel_cropping_large_offsets.t \
    processing/geometry/0011_ppm_inspection_aggressive_scaling.t \
    processing/geometry/0012_resize_precision_mode_run.t \
    processing/geometry/0013_resize_precision_mode_scaler_input.t \
    processing/geometry/0014_resize_precision_mode_post_colorspace.t \
    processing/geometry/0015_resample_nearest_downscale_80pct_lsqa.t \
    processing/geometry/0016_resample_gaussian_downscale_80pct_lsqa.t \
    processing/geometry/0017_resample_hanning_downscale_80pct_lsqa.t \
    processing/geometry/0018_resample_hamming_downscale_80pct_lsqa.t \
    processing/geometry/0019_resample_bilinear_downscale_80pct_lsqa.t \
    processing/geometry/0020_resample_welsh_downscale_80pct_lsqa.t \
    processing/geometry/0021_resample_bicubic_downscale_80pct_lsqa.t \
    processing/geometry/0022_resample_lanczos2_downscale_80pct_lsqa.t \
    processing/geometry/0023_resample_lanczos3_downscale_80pct_lsqa.t \
    processing/geometry/0024_resample_lanczos4_downscale_80pct_lsqa.t \
    processing/geometry/0025_resample_nearest_upscale_120pct_lsqa.t \
    processing/geometry/0026_resample_gaussian_upscale_120pct_lsqa.t \
    processing/geometry/0027_resample_hanning_upscale_120pct_lsqa.t \
    processing/geometry/0028_resample_hamming_upscale_120pct_lsqa.t \
    processing/geometry/0029_resample_bilinear_upscale_120pct_lsqa.t \
    processing/geometry/0030_resample_welsh_upscale_120pct_lsqa.t \
    processing/geometry/0031_resample_bicubic_upscale_120pct_lsqa.t \
    processing/geometry/0032_resample_lanczos2_upscale_120pct_lsqa.t \
    processing/geometry/0033_resample_lanczos3_upscale_120pct_lsqa.t \
    processing/geometry/0034_resample_lanczos4_upscale_120pct_lsqa.t \
    processing/geometry/0035_resize_height_32_lsqa.t \
    processing/geometry/0036_resize_height_32px_lsqa.t \
    processing/geometry/0037_resize_height_80pct_lsqa.t \
    processing/geometry/0038_resize_height_100_lsqa.t \
    processing/geometry/0039_resize_height_100px_lsqa.t \
    processing/geometry/0040_resize_height_120pct_lsqa.t \
    io/loader/0001_progressive_jpeg.t \
    io/loader/0002_pngsuite_basic_convert.t \
    io/loader/0003_pngsuite_basic_width32.t \
    io/loader/0004_pngsuite_basic_cropped.t \
    io/loader/0005_pngsuite_background_convert.t \
    io/loader/0006_pngsuite_background_white.t \
    io/loader/0007_pngsuite_background_width32_white.t \
    io/loader/0008_loader_coregraphics_pixelformat.t \
    io/loader/0009_loader_wic_pixelformat.t \
    io/loader/0010_loader_gdk_pixbuf_pixelformat.t \
    io/loader/0011_loader_gd_pixelformat.t \
    io/loader/0012_loader_libpng_pixelformat.t \
    io/loader/0013_loader_libjpeg_pixelformat.t \
    io/loader/0014_loader_builtin_pixelformat.t \
    io/loader/0015_loader_quicklook_pixelformat.t \
    io/loader/0016_loader_gnome_thumbnailer_pixelformat.t \
    io/loader/0017_loader_libwebp_pixelformat.t \
    io/loader/0018_loader_builtin_psd.t \
    io/loader/0019_loader_builtin_hdr.t \
    io/loader/0020_loader_builtin_pic.t \
    quant/mapfile/0001_mapfile_export_act.t \
    quant/mapfile/0002_mapfile_export_pal_default.t \
    quant/mapfile/0003_mapfile_export_pal_stdout.t \
    quant/mapfile/0004_mapfile_export_riff.t \
    quant/mapfile/0005_mapfile_export_gpl.t \
    quant/mapfile/0006_mapfile_import_gpl_prefixed.t \
    quant/mapfile/0007_mapfile_import_act_extension.t \
    quant/mapfile/0008_mapfile_import_riff_explicit.t \
    quant/mapfile/0009_mapfile_import_stdin.t \
    io/network/0001_curl_invalid_file_url.t \
    io/network/0002_curl_invalid_https_url.t \
    io/network/0003_curl_file_scheme.t \
    io/network/0004_curl_self_signed_blocked.t \
    io/network/0005_curl_self_signed_with_k.t \
    codec/output/0001_vt340_colour_sequences.t \
    codec/output/0002_vt340_monochrome_sequences.t \
    codec/output/0003_direct_sixel_encode.t \
    codec/output/0004_sixel_metadata_inspection.t \
    codec/output/0005_monochrome_frame_trailer.t \
    codec/output/0006_two_pass_sixel_conversion.t \
    packaging/0001_python_wheel_install.t \
    packaging/0002_python_wheel_roundtrip.t \
    quant/palette/init/0001_kmeans_init_default_unset.t \
    quant/palette/init/0002_kmeans_init_auto.t \
    quant/palette/init/0003_kmeans_init_unknown.t \
    quant/palette/init/0004_kmeans_init_pca.t \
    quant/palette/init/0005_kmeans_init_cache.t \
    quant/palette/fallback/0006_palette_fallback_conversion.t \
    quant/palette/fallback/0007_palette_fallback_output.t \
    quant/palette/usage/0008_select_color_ambiguous_prefix_rejected.t \
    quant/palette/usage/0009_select_color_unique_prefix_accepted.t \
    quant/palette/usage/0010_explicit_dimensions_with_palette.t \
    quant/palette/usage/0011_fast_encoder_external_palette.t \
    quant/palette/usage/0012_palette16_conversion.t \
    quant/palette/usage/0013_xterm_palette_override_repetition.t \
    quant/palette/usage/0014_tiff_palette_filters.t \
    quant/palette/usage/0015_grayscale_png_with_palette.t \
    quant/palette/usage/0016_apply_grayscale_palette_to_colour_png.t \
    quant/palette/usage/0017_stacked_palette_files.t \
    quant/palette/usage/0018_xterm16_palette_preset.t \
    quant/palette/usage/0019_jpeg_palette_welsh.t \
    quant/palette/usage/0020_ppm_palette16_conversion.t \
    quant/palette/usage/0021_lanczos2_palette_limit.t \
    quant/palette/usage/0022_lanczos3_two_colour_palette.t \
    quant/palette/usage/0023_lanczos4_palette_dump.t \
    quant/palette/usage/0024_interlaced_palette_dump.t \
    quant/palette/usage/0025_lookup_policy_vpte_float32.t \
    quant/palette/usage/0026_merge_kmeans_float32_lsqa.t \
    quant/palette/usage/0027_merge_heckbert_float32_lsqa.t \
    quant/palette/usage/0028_merge_kmeans_8bit_lsqa.t \
    quant/palette/usage/0029_merge_heckbert_8bit_lsqa.t \
    quant/palette/usage/0030_snap_kmeans_float32_lsqa.t \
    quant/palette/usage/0031_snap_heckbert_float32_lsqa.t \
    quant/palette/usage/0032_snap_kmeans_8bit_lsqa.t \
    quant/palette/usage/0033_snap_heckbert_8bit_lsqa.t \
    quant/palette/usage/0034_lookup_policy_1d_eytzinger_8bit.t \
    quant/palette/usage/0035_lookup_policy_1d_eytzinger_float32.t \
    quant/palette/usage/0036_lookup_policy_1d_eytzinger_float32_gamma_lsqa.t \
    quant/palette/usage/0037_lookup_policy_1d_eytzinger_float32_linear_lsqa.t \
    quant/palette/usage/0038_lookup_policy_1d_eytzinger_float32_oklab_lsqa.t \
    quant/palette/usage/0039_lookup_policy_1d_eytzinger_float32_cielab_lsqa.t \
    quant/palette/usage/0040_lookup_policy_1d_eytzinger_float32_din99d_lsqa.t \
    quant/palette/usage/0041_lookup_policy_vpte_8bit_gamma_lsqa.t \
    quant/palette/usage/0042_lookup_policy_vpte_float32_gamma_lsqa.t \
    quant/palette/usage/0043_lookup_policy_vpte_float32_linear_lsqa.t \
    quant/palette/usage/0044_lookup_policy_vpte_float32_oklab_lsqa.t \
    quant/palette/usage/0045_lookup_policy_vpte_float32_cielab_lsqa.t \
    quant/palette/usage/0046_lookup_policy_vpte_float32_din99d_lsqa.t \
    quant/palette/usage/0047_colorspace_rgb_cielab_lsqa.t \
    quant/palette/usage/0048_colorspace_hls_oklab_lsqa.t \
    quant/palette/usage/0049_colorspace_hls_vs_rgb_lsqa.t \
    quant/palette/usage/0050_palette_map_png_rgb.t \
    quant/palette/usage/0051_palette_map_png_pal1.t \
    quant/palette/usage/0052_palette_map_png_pal2.t \
    quant/palette/usage/0053_palette_map_png_pal4.t \
    quant/palette/usage/0054_palette_map_png_pal8.t \
    quant/palette/usage/0055_palette_map_png_g1.t \
    quant/palette/usage/0056_palette_map_png_g2.t \
    quant/palette/usage/0057_palette_map_png_g4.t \
    quant/palette/usage/0058_palette_map_png_g8.t \
    quant/palette/usage/0059_clustering_colorspace_gamma_lsqa.t \
    quant/palette/usage/0060_clustering_colorspace_linear_lsqa.t \
    quant/palette/usage/0061_clustering_colorspace_oklab_lsqa.t \
    quant/palette/usage/0062_clustering_colorspace_cielab_lsqa.t \
    quant/palette/usage/0063_clustering_colorspace_din99d_lsqa.t \
    quant/palette/usage/0064_clustering_gamma_working_gamma_lsqa.t \
    quant/palette/usage/0065_clustering_gamma_working_linear_lsqa.t \
    quant/palette/usage/0066_clustering_gamma_working_oklab_lsqa.t \
    quant/palette/usage/0067_clustering_gamma_working_cielab_lsqa.t \
    quant/palette/usage/0068_clustering_gamma_working_din99d_lsqa.t \
    quant/palette/usage/0069_clustering_linear_working_gamma_lsqa.t \
    quant/palette/usage/0070_clustering_linear_working_linear_lsqa.t \
    quant/palette/usage/0071_clustering_linear_working_oklab_lsqa.t \
    quant/palette/usage/0072_clustering_linear_working_cielab_lsqa.t \
    quant/palette/usage/0073_clustering_linear_working_din99d_lsqa.t \
    quant/palette/usage/0074_clustering_oklab_working_gamma_lsqa.t \
    quant/palette/usage/0075_clustering_oklab_working_linear_lsqa.t \
    quant/palette/usage/0076_clustering_oklab_working_oklab_lsqa.t \
    quant/palette/usage/0077_clustering_oklab_working_cielab_lsqa.t \
    quant/palette/usage/0078_clustering_oklab_working_din99d_lsqa.t \
    quant/palette/usage/0079_clustering_cielab_working_gamma_lsqa.t \
    quant/palette/usage/0080_clustering_cielab_working_linear_lsqa.t \
    quant/palette/usage/0081_clustering_cielab_working_oklab_lsqa.t \
    quant/palette/usage/0082_clustering_cielab_working_cielab_lsqa.t \
    quant/palette/usage/0083_clustering_cielab_working_din99d_lsqa.t \
    quant/palette/usage/0084_clustering_din99d_working_gamma_lsqa.t \
    quant/palette/usage/0085_clustering_din99d_working_linear_lsqa.t \
    quant/palette/usage/0086_clustering_din99d_working_oklab_lsqa.t \
    quant/palette/usage/0087_clustering_din99d_working_cielab_lsqa.t \
    quant/palette/usage/0088_clustering_din99d_working_din99d_lsqa.t \
    quant/palette/usage/0089_lookup_policy_vptree_8bit.t \
    quant/palette/usage/0090_lookup_policy_vptree_float32.t \
    planner/pipeline/0001_pipeline_planner_baseline_run.t \
    planner/pipeline/0002_pipeline_planner_baseline_summary.t \
    planner/pipeline/0003_pipeline_planner_baseline_threads.t \
    planner/pipeline/0004_pipeline_planner_overrides_run.t \
    planner/pipeline/0005_pipeline_planner_overrides_summary.t \
    planner/pipeline/0006_pipeline_planner_overrides_threads.t \
    security/issue/0001_issue167_empty_bg.t \
    security/issue/0002_issue167_height.t \
    security/issue/0003_issue166_width.t \
    security/issue/0004_issue200_heap_overflow.t \
    security/issue/0005_issue131_img2sixel_gif.t \
    security/issue/0006_issue166_height.t \
    security/issue/0007_issue167_height_option_first.t \
    security/issue/0008_issue200_heap_overflow_reject.t \
    security/issue/0009_issue207_resize_poc.t \
    security/issue/0010_issue51_stbi_1561_poc.t \
    security/issue/0011_issue73_stbi_1561_poc.t \
    diagnostics/probe/0001_probe_parse.t \
    bindings/python/0001_python_bindings.t \
    bindings/python/0002_python_api_formats.t \
    bindings/python/0003_python_api_options.t \
    bindings/python/0004_python_api_errors.t \
    bindings/python/0005_python_resource_management.t \
    quality_gate/ms_ssim/0005_lsqa_format_grayscale.t \
    quality_gate/ms_ssim/0006_lsqa_format_palette.t \
    quality_gate/ms_ssim/0007_lsqa_format_rgb.t \
    quality_gate/ms_ssim/0015_lsqa_repeat_palette_variance.t \
    quality_gate/ms_ssim/0016_lsqa_corrupted_metadata_noise.t \
    quality_gate/ms_ssim/0017_lsqa_corrupted_truncated_png.t \
    quality_gate/ms_ssim/0018_lsqa_format_tga_type1_pal4.t \
    quality_gate/ms_ssim/0019_lsqa_format_tga_type1_pal8.t \
    quality_gate/ms_ssim/0020_lsqa_format_tga_type2_rgb.t \
    quality_gate/ms_ssim/0021_lsqa_format_tga_type3_gray.t \
    quality_gate/ms_ssim/0022_lsqa_format_tga_type9_pal4.t \
    quality_gate/ms_ssim/0023_lsqa_format_tga_type9_pal8.t \
    quality_gate/ms_ssim/0024_lsqa_format_tga_type10_rgb.t \
    quality_gate/ms_ssim/0025_lsqa_format_tga_type11_gray.t \
    quality_gate/ms_ssim/0026_lsqa_roundtrip_rgba_small.t \
    quality_gate/ms_ssim/0027_lsqa_format_tiff_rgb_small.t \
    quality_gate/ms_ssim/0028_lsqa_format_tiff_palette_small.t \
    quality_gate/ms_ssim/0029_lsqa_format_tiff_grayscale.t \
    quality_gate/ms_ssim/0030_lsqa_format_tiff_rgba.t \
    quality_gate/ms_ssim/0031_lsqa_format_tiff_lzw.t \
    quality_gate/ms_ssim/0032_lsqa_format_tiff_tiled.t \
    cli/sixel2png/0001_version.t \
    cli/sixel2png/0002_help.t \
    cli/sixel2png/0002_missing_args.t \
    cli/sixel2png/0003_unknown_option.t \
    cli/sixel2png/0004_invalid_decoder_value.t \
    cli/sixel2png/0005_default_output.t

SERIAL_TESTS = \
    cli/aborttrace/0001_img2sixel_aborttrace.t

COPY_TESTS = $(TESTS)
check_PROGRAMS = test_runner

TEST_EXTENSIONS = .t

HELPER_FILES = \
    _lib/sh/common.sh \
    docs/consistency/list_envvars.sh \
    lib/sh/packaging/python_wheel_common.sh \
    lib/sh/loader/pngsuite_common.sh \
    lib/sh/python/common.sh

ARTIFACT_ROOT = $(abs_builddir)/_artifacts
AM_TESTS_ENVIRONMENT = ARTIFACT_ROOT='$(ARTIFACT_ROOT)'; export ARTIFACT_ROOT;
AM_TESTS_ENVIRONMENT += TOP_SRCDIR='$(abs_top_srcdir)'; export TOP_SRCDIR;
AM_TESTS_ENVIRONMENT += TOP_BUILDDIR='$(abs_top_builddir)'; export TOP_BUILDDIR;
# Keep executable suffix in sync with the configured target so TAP
# wrappers can resolve cross-built Windows binaries under Wine.
AM_TESTS_ENVIRONMENT += SIXEL_BIN_EXT='@EXEEXT@'; export SIXEL_BIN_EXT;
AM_TESTS_ENVIRONMENT += SIXEL_TSAN_BUILD='@SIXEL_TSAN_BUILD@'; \
       export SIXEL_TSAN_BUILD;
AM_TESTS_ENVIRONMENT += SIXEL_MSAN_BUILD='@SIXEL_MSAN_BUILD@'; \
       export SIXEL_MSAN_BUILD;
AM_TESTS_ENVIRONMENT += CROSS_COMPILING='@cross_compiling@'; \
       export CROSS_COMPILING;
AM_TESTS_ENVIRONMENT += ENABLE_PYTHON='@ENABLE_PYTHON@'; export ENABLE_PYTHON;
AM_TESTS_ENVIRONMENT += ENABLE_PYTHON_WHEEL='@ENABLE_PYTHON_WHEEL@'; \
       export ENABLE_PYTHON_WHEEL;
AM_TESTS_ENVIRONMENT += RUNTIME_SHLIBPATH_VAR='@SIXEL_LIBTOOL_SHLIBPATH_VAR@'; \
       export RUNTIME_SHLIBPATH_VAR;
AM_TESTS_ENVIRONMENT += RUNTIME_SHLIBPATH_SEP='@SIXEL_LIBTOOL_PATH_SEPARATOR@'; \
       export RUNTIME_SHLIBPATH_SEP;
TESTS_COMMON_CPPFLAGS_VAL = $(AM_CPPFLAGS) \
       -I$(top_builddir) -I$(top_builddir)/include \
       -I$(top_srcdir)/include -I$(top_srcdir)/src \
       -I$(top_srcdir)
if WANT_AMALGAMATED_TOOLS
TESTS_COMMON_CFLAGS_VAL = $(AM_CFLAGS) \
        $(LIBCURL_CFLAGS) \
        $(WINHTTP_CFLAGS) \
        $(LIBPNG_CFLAGS) \
        $(LIBTIFF_CFLAGS) \
        $(LIBJPEG_CFLAGS) \
        $(LIBWEBP_CFLAGS) \
        $(GDK_PIXBUF_CFLAGS) \
        $(GD_CFLAGS) \
        $(WIC_CFLAGS) \
        $(COREGRAPHICS_CFLAGS) \
        $(QUICKLOOK_CFLAGS) \
        $(CLIPBOARD_CFLAGS) \
        @THREAD_CFLAGS@ \
        $(MAYBE_COVERAGE)
else
TESTS_COMMON_CFLAGS_VAL = $(AM_CFLAGS) \
        $(MAYBE_COVERAGE)
endif
TESTS_COMMON_LDFLAGS_VAL = $(AM_LDFLAGS) $(MAYBE_COVERAGE)
if WANT_AMALGAMATED_TOOLS
TESTS_COMMON_LDADD_VAL = -lm \
        $(LIBS) \
        $(LIBCURL_LIBS) \
        $(WINHTTP_LIBS) \
        $(LIBPNG_LIBS) \
        $(LIBTIFF_LIBS) \
        $(LIBJPEG_LIBS) \
        $(LIBWEBP_LIBS) \
        $(GDK_PIXBUF_LIBS) \
        $(GD_LIBS) \
        $(WIC_LIBS) \
        $(COREGRAPHICS_LIBS) \
        $(QUICKLOOK_LIBS) \
        $(CLIPBOARD_LIBS) \
        $(ABORTTRACE_LIBS) \
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
TESTS_AMALGAMATION_LDADD_COMMON = -lm \
        $(LIBS) \
        $(LIBCURL_LIBS) \
        $(WINHTTP_LIBS) \
        $(LIBPNG_LIBS) \
        $(LIBTIFF_LIBS) \
        $(LIBJPEG_LIBS) \
        $(LIBWEBP_LIBS) \
        $(GDK_PIXBUF_LIBS) \
        $(GD_LIBS) \
        $(WIC_LIBS) \
        $(COREGRAPHICS_LIBS) \
        $(QUICKLOOK_LIBS) \
        $(CLIPBOARD_LIBS) \
        $(ABORTTRACE_LIBS) \
        @THREAD_LIBS@
else
TESTS_AMALGAMATION_LDADD_COMMON = $(top_builddir)/src/libsixel.la $(LIBS)
endif

if WANT_AMALGAMATED_TOOLS
nodist_test_runner_SOURCES = $(AMALGAMATION_SIXEL)
TEST_RUNNER_AMALGAMATION_DEFINES = \
    -DBUILD_TEST_RUNNER \
    -DBUILD_SIXEL_PIXBUF_LOADER \
    -DBUILD_TEST_0001_FILTER_CLIP \
    -DBUILD_TEST_0001_GDK_PIXBUF_LOADER \
    -DBUILD_TEST_0001_IMG2SIXEL_ABORTTRACE \
    -DBUILD_TEST_0001_KMEANS_INIT \
    -DBUILD_TEST_0001_PROBE_PARSE \
    -DBUILD_TEST_0002_FILTER_SAMPLE \
    -DBUILD_TEST_0002_INCREMENTAL_LOAD \
    -DBUILD_TEST_0003_CORRUPT_DATA \
    -DBUILD_TEST_0003_FILTER_RESIZE \
    -DBUILD_TEST_0004_FILTER_COLORS \
    -DBUILD_TEST_0004_PROPAGATE_ERROR \
    -DBUILD_TEST_0005_CONTEXT_FREE \
    -DBUILD_TEST_0005_FILTER_LOOKUP \
    -DBUILD_TEST_0006_FILTER_FINAL_MERGE \
    -DBUILD_TEST_0006_LOADER_MODULE \
    -DBUILD_TEST_0007_FILTER_VPTE \
    -DBUILD_TEST_0007_LOADER_PLUGIN_INTEGRATION \
    -DBUILD_TEST_0008_FILTER_LOAD \
    -DBUILD_TEST_0008_LOADER_COREGRAPHICS_PIXELFORMAT \
    -DBUILD_TEST_0009_FILTER_DITHER \
    -DBUILD_TEST_0009_LOADER_WIC_PIXELFORMAT \
    -DBUILD_TEST_0010_FILTER_ENCODE \
    -DBUILD_TEST_0010_LOADER_GDK_PIXBUF_PIXELFORMAT \
    -DBUILD_TEST_0011_LOADER_GD_PIXELFORMAT \
    -DBUILD_TEST_0012_LOADER_LIBPNG_PIXELFORMAT \
    -DBUILD_TEST_0013_LOADER_LIBJPEG_PIXELFORMAT \
    -DBUILD_TEST_0014_LOADER_BUILTIN_PIXELFORMAT \
    -DBUILD_TEST_0015_LOADER_QUICKLOOK_PIXELFORMAT \
    -DBUILD_TEST_0016_LOADER_GNOME_THUMBNAILER_PIXELFORMAT \
    -DBUILD_TEST_0017_LOADER_LIBWEBP_PIXELFORMAT \
    -DBUILD_TEST_0029_CLI_TOKEN_IS_KNOWN_OPTION \
    -DBUILD_TEST_0030_CLI_OPTION_REQUIRES_ARGUMENT \
    -DBUILD_TEST_0031_CLI_GUARD_MISSING_ARGUMENT \
    -DBUILD_ABORTTRACE \
    -DBUILD_CLI
test_runner_CPPFLAGS = $(TESTS_AMALGAMATION_CPPFLAGS_COMMON) \
    $(TEST_RUNNER_AMALGAMATION_DEFINES) \
    $(GDK_PIXBUF_CFLAGS) \
    -DGDK_PIXBUF_ENABLE_BACKEND \
    -DSIXEL_PIXBUF_LOADER_TESTING \
    -DSIXEL_ENABLE_GDK_PIXBUF_LOADER_TESTS
test_runner_CFLAGS = $(TESTS_AMALGAMATION_CFLAGS_COMMON)
test_runner_LDFLAGS = $(TESTS_AMALGAMATION_LDFLAGS_COMMON)
test_runner_LDADD = $(TESTS_AMALGAMATION_LDADD_COMMON)
else
test_runner_SOURCES = test_runner.c \
    cli/aborttrace/0001_img2sixel_aborttrace.c \
    cli/0029_cli_token_is_known_option.c \
    cli/0030_cli_option_requires_argument.c \
    cli/0031_cli_guard_missing_argument.c \
    processing/filter/0001_filter_clip.c \
    processing/filter/0002_filter_sample.c \
    processing/filter/0003_filter_resize.c \
    processing/filter/0004_filter_colors.c \
    processing/filter/0005_filter_lookup.c \
    processing/filter/0006_filter_final_merge.c \
    processing/filter/0007_filter_vpte.c \
    processing/filter/0008_filter_load.c \
    processing/filter/0009_filter_dither.c \
    processing/filter/0010_filter_encode.c \
    processing/filter/filter_test_common.h \
    io/loader/0008_loader_coregraphics_pixelformat.c \
    io/loader/0009_loader_wic_pixelformat.c \
    io/loader/0010_loader_gdk_pixbuf_pixelformat.c \
    io/loader/0011_loader_gd_pixelformat.c \
    io/loader/0012_loader_libpng_pixelformat.c \
    io/loader/0013_loader_libjpeg_pixelformat.c \
    io/loader/0014_loader_builtin_pixelformat.c \
    io/loader/0015_loader_quicklook_pixelformat.c \
    io/loader/0016_loader_gnome_thumbnailer_pixelformat.c \
    io/loader/0017_loader_libwebp_pixelformat.c \
    io/loader/pixelformat_test_common.h \
    quant/palette/0001_kmeans_init.c \
    diagnostics/probe/0001_probe_parse.c \
    extension/gdk-pixbuf-loader/0001_gdk_pixbuf_loader.c \
    extension/gdk-pixbuf-loader/0002_incremental_load.c \
    extension/gdk-pixbuf-loader/0003_corrupt_data.c \
    extension/gdk-pixbuf-loader/0004_propagate_error.c \
    extension/gdk-pixbuf-loader/0005_context_free.c \
    extension/gdk-pixbuf-loader/0006_loader_module.c \
    extension/gdk-pixbuf-loader/0007_loader_plugin_integration.c \
    $(top_srcdir)/gdk-pixbuf-loader/sixel-pixbuf-loader.c \
    $(top_srcdir)/converters/aborttrace.c \
    $(top_srcdir)/converters/cli.c
test_runner_CPPFLAGS = $(TESTS_COMMON_CPPFLAGS_VAL) \
    $(GDK_PIXBUF_CFLAGS) \
    -DGDK_PIXBUF_ENABLE_BACKEND \
    -DSIXEL_PIXBUF_LOADER_TESTING \
    -DSIXEL_ENABLE_GDK_PIXBUF_LOADER_TESTS
test_runner_CFLAGS = $(TESTS_COMMON_CFLAGS_VAL)
test_runner_LDFLAGS = $(TESTS_COMMON_LDFLAGS_VAL)
test_runner_LDADD = $(TESTS_COMMON_LDADD_VAL) $(GDK_PIXBUF_LIBS) $(ABORTTRACE_LIBS)
endif

LOG_DRIVER = $(T_LOG_DRIVER)
LOG_DRIVER_FLAGS = $(AM_TEST_LOG_DRIVER_FLAGS)
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_builddir)/build-aux/lso-tap-driver.sh
AM_TEST_LOG_DRIVER_FLAGS = --comments

# Default to soft errors so the harness continues after individual failures.
DISABLE_HARD_ERRORS = yes
AM_TEST_TIMEOUT = 90
AM_LOG_DRIVER_FLAGS = --timeout=$(AM_TEST_TIMEOUT)
# Leave AM_LOG_FLAGS empty so Automake does not append timeout parameters to
# LOG_COMPILE; the drivers consume timeout hints themselves.
AM_LOG_FLAGS =

# Override check-TESTS to avoid nested single quotes in automake helpers.
# Export config.h feature toggles once so tests can rely on environment
# variables instead of repeatedly reading the header.
check-TESTS: $(check_PROGRAMS)
	@$(am__rm_f) $(RECHECK_LOGS)
	@$(am__rm_f) $(RECHECK_LOGS:.log=.trs)
	@$(am__rm_f) $(TEST_SUITE_LOG)
	@set +e; \
	config_header="$(top_builddir)/config.h"; \
	resolve_test_tool_path() { \
	    top_path=$$1; \
	    real_path=$$2; \
	    magic=""; \
	    if [ -f "$$top_path" ]; then \
	        set -- $$(dd if="$$top_path" bs=1 count=2 2>/dev/null | \
	                 od -An -tx1); \
	        magic="$${1-}$${2-}"; \
	        if [ "x$$magic" = "x2321" ] && [ -f "$$real_path" ]; then \
	            printf '%s\n' "$$real_path"; \
	        else \
	            printf '%s\n' "$$top_path"; \
	        fi; \
	        return 0; \
	    fi; \
	    if [ -f "$$real_path" ]; then \
	        printf '%s\n' "$$real_path"; \
	    else \
	        printf '%s\n' "$$top_path"; \
	    fi; \
	    return 0; \
	}; \
	IMG2SIXEL_PATH=$$(resolve_test_tool_path \
	    "$(abs_top_builddir)/converters/img2sixel@EXEEXT@" \
	    "$(abs_top_builddir)/converters/@SIXEL_LIBTOOL_OBJDIR@/img2sixel@EXEEXT@"); \
	SIXEL2PNG_PATH=$$(resolve_test_tool_path \
	    "$(abs_top_builddir)/converters/sixel2png@EXEEXT@" \
	    "$(abs_top_builddir)/converters/@SIXEL_LIBTOOL_OBJDIR@/sixel2png@EXEEXT@"); \
	LSQA_PATH=$$(resolve_test_tool_path \
	    "$(abs_top_builddir)/assessment/lsqa@EXEEXT@" \
	    "$(abs_top_builddir)/assessment/@SIXEL_LIBTOOL_OBJDIR@/lsqa@EXEEXT@"); \
	TEST_RUNNER_PATH=$$(resolve_test_tool_path \
	    "$(abs_top_builddir)/tests/test_runner@EXEEXT@" \
	    "$(abs_top_builddir)/tests/@SIXEL_LIBTOOL_OBJDIR@/test_runner@EXEEXT@"); \
	export IMG2SIXEL_PATH SIXEL2PNG_PATH LSQA_PATH TEST_RUNNER_PATH; \
	\
	if [ -f "$$config_header" ]; then \
	    for feature in `awk '/^#define [A-Z0-9_]+ 1$$/ {print $$2}' "$$config_header"`; do \
	        eval "$$feature=1"; \
	        export $$feature; \
	    done; \
	    for feature in `awk '/^\/\* #undef [A-Z0-9_]+/ {print $$3}' "$$config_header"`; do \
	        eval "$$feature=0"; \
	        export $$feature; \
	    done; \
	fi; \
	log_list=""; \
	for i in $(TEST_LOGS); do \
	    log_list="$$log_list $$i"; \
	done; \
	log_list=`echo $$log_list`; \
	$(MAKE) $(AM_MAKEFLAGS) $(TEST_SUITE_LOG) \
	    TEST_LOGS="$$log_list"; \
	exit $$?;

EXTRA_DIST = $(TESTS) $(HELPER_FILES) \
    data/inputs/formats/grayscale.jpg \
    data/inputs/formats/grayscale.tiff \
    data/inputs/formats/palette.png \
    data/inputs/formats/rgb.png \
    data/inputs/formats/rgb-lzw.tiff \
    data/inputs/formats/rgb-tiled.tiff \
    data/inputs/formats/snake-64-reference-rgb-flip.png \
    data/inputs/formats/snake-64-reference-rgb.png \
    data/inputs/formats/snake-64-reference-gray-flip.png \
    data/inputs/formats/snake-64-reference-gray.png \
    data/inputs/formats/snake-64-reference-rgba.png \
    data/inputs/formats/snake-64-grayscale.jpg \
    data/inputs/formats/rgba.png \
    data/inputs/formats/rgba.tiff \
    data/inputs/formats/snake-tga-type1-pal4.tga \
    data/inputs/formats/snake-tga-type1-pal8.tga \
    data/inputs/formats/snake-tga-type2-rgb.tga \
    data/inputs/formats/snake-tga-type3-gray.tga \
    data/inputs/formats/snake-tga-type9-pal4.tga \
    data/inputs/formats/snake-tga-type9-pal8.tga \
    data/inputs/formats/snake-tga-type10-rgb.tga \
    data/inputs/formats/snake-tga-type11-gray.tga \
    data/inputs/formats/stbi_minimal.hdr \
    data/inputs/formats/stbi_minimal.psd \
    data/inputs/formats/stbi_minimal.pic \
    data/inputs/small.ppm \
    data/inputs/tall.ppm \
    data/inputs/small.gif \
    data/inputs/snake_64.bmp \
    data/inputs/snake_64.gif \
    data/inputs/snake_64.jpg \
    data/inputs/snake_64.pbm \
    data/inputs/snake_64.pgm \
    data/inputs/snake_64.png \
    data/inputs/snake_64.ppm \
    data/inputs/snake_64.six \
    data/inputs/snake_64-oversized.six \
    data/inputs/snake_64.tga \
    data/inputs/snake_64.tiff \
    data/inputs/snake_64-indexed.tiff \
    data/inputs/snake_64.webp \
    data/inputs/scaling/snake_64_80pct.png \
    data/inputs/scaling/snake_64_120pct.png \
    data/inputs/scaling/snake_64_h32.png \
    data/inputs/scaling/snake_64_h32px.png \
    data/inputs/scaling/snake_64_h80pct.png \
    data/inputs/scaling/snake_64_h100.png \
    data/inputs/scaling/snake_64_h100px.png \
    data/inputs/scaling/snake_64_h120pct.png \
    data/inputs/scaling/snake_64_nearest_80pct.png \
    data/inputs/scaling/snake_64_nearest_120pct.png \
    data/inputs/scaling/snake_64_gaussian_80pct.png \
    data/inputs/scaling/snake_64_gaussian_120pct.png \
    data/inputs/scaling/snake_64_hanning_80pct.png \
    data/inputs/scaling/snake_64_hanning_120pct.png \
    data/inputs/scaling/snake_64_hamming_80pct.png \
    data/inputs/scaling/snake_64_hamming_120pct.png \
    data/inputs/scaling/snake_64_bilinear_80pct.png \
    data/inputs/scaling/snake_64_bilinear_120pct.png \
    data/inputs/scaling/snake_64_welsh_80pct.png \
    data/inputs/scaling/snake_64_welsh_120pct.png \
    data/inputs/scaling/snake_64_bicubic_80pct.png \
    data/inputs/scaling/snake_64_bicubic_120pct.png \
    data/inputs/scaling/snake_64_lanczos2_80pct.png \
    data/inputs/scaling/snake_64_lanczos2_120pct.png \
    data/inputs/scaling/snake_64_lanczos3_80pct.png \
    data/inputs/scaling/snake_64_lanczos3_120pct.png \
    data/inputs/scaling/snake_64_lanczos4_80pct.png \
    data/inputs/scaling/snake_64_lanczos4_120pct.png \
    data/resolutions/hd_landscape.png \
    data/resolutions/panorama_tall.png \
    data/resolutions/panorama_wide.png \
    data/resolutions/quarter_square.png \
    data/resolutions/svga_landscape.png \
    data/resolutions/tiny_square.png \
    data/corrupted/metadata_noise.jpg \
    data/corrupted/truncated.png \
    security/issue/data/166/poc \
    security/issue/data/167/poc \
    security/issue/data/200/POC_img2sixel_heap_buffer_overflow \
    security/issue/data/207/poc \
    security/issue/data/libsixel-libsixel/51/stbi_1561_poc.bin \
    security/issue/data/libsixel-libsixel/73/stbi_1561_poc.bin \
    security/issue/data/131/2020-01-30-img2sixel.gif \
    security/issue/data/136/poc \
    io/network/certs/server.crt \
    io/network/certs/server.key

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
