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
    aborttrace/t/0001_img2sixel_aborttrace.t \
    animation/t/0001_disable_update_mode.t \
    animation/t/0002_static_frame_render.t \
    animation/t/0003_update_and_static_frame.t \
    animation/t/0004_split_sequence_atkinson.t \
    cli-argument-shift/t/0001_invalid_positional.t \
    cli-argument-shift/t/0002_argument_shift_missing_mapfile.t \
    cli-argument-shift/t/0003_argument_shift_outfile_named_like_option.t \
    cli-argument-shift/t/0005_cli_token_is_known_option.t \
    cli-argument-shift/t/0006_cli_option_requires_argument.t \
    cli-argument-shift/t/0007_cli_guard_missing_argument.t \
    cli-core/t/0001_help.t \
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
    cli-option-invalid/t/0001a_invalid_option_arguments_unknown_dither.t \
    cli-option-invalid/t/0001b_invalid_option_arguments_unknown_resize_filter.t \
    cli-option-invalid/t/0001c_invalid_option_arguments_unknown_scaling_mode.t \
    cli-option-invalid/t/0001d_invalid_option_arguments_unknown_tone_adjustment.t \
    cli-option-invalid/t/0001e_invalid_option_arguments_invalid_width.t \
    cli-option-invalid/t/0001f_invalid_option_arguments_invalid_height.t \
    cli-option-invalid/t/0001g_invalid_option_arguments_invalid_format.t \
    cli-option-invalid/t/0001h_invalid_option_arguments_invalid_quality.t \
    cli-option-invalid/t/0001i_invalid_option_arguments_invalid_layout.t \
    cli-option-invalid/t/0001j_invalid_option_arguments_invalid_bpp.t \
    cli-option-invalid/t/0001k_invalid_option_arguments_invalid_encoder_tweak.t \
    cli-option-invalid/t/0001l_invalid_option_arguments_invalid_background_colour.t \
    cli-option-invalid/t/0001m_invalid_option_arguments_missing_background_component.t \
    cli-option-invalid/t/0001n_invalid_option_arguments_overlong_background.t \
    cli-option-invalid/t/0001o_invalid_option_arguments_malformed_hex_background.t \
    cli-option-invalid/t/0001p_invalid_option_arguments_unknown_named_colour.t \
    cli-option-invalid/t/0001q_invalid_option_arguments_incomplete_rgb_background.t \
    cli-option-invalid/t/0001r_invalid_option_arguments_unsupported_legacy_width.t \
    cli-option-invalid/t/0001s_invalid_option_arguments_missing_palette_file.t \
    cli-option-invalid/t/0001t_invalid_option_arguments_invalid_colour_space_index.t \
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
    cli-option-invalid/t/0022a_invalid_option_combinations_palette_and_encode.t \
    cli-option-invalid/t/0022b_invalid_option_combinations_inspect_with_palette.t \
    cli-option-invalid/t/0022c_invalid_option_combinations_palette_with_terminal_preset.t \
    cli-option-invalid/t/0022d_invalid_option_combinations_eight_bit_with_palette_dump.t \
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
    cli-options/t/0001_long_option_forms.t \
    cli-options/t/0002_ormode_option_runs.t \
    cli-options/t/0003_gri_limit_lsqa.t \
    clipboard/t/0001_clipboard_basic.t \
    completion/t/0021_completion_show_bash.t \
    completion/t/0022_completion_show_zsh.t \
    completion/t/0023_completion_install_bash.t \
    completion/t/0024_completion_install_zsh.t \
    completion/t/0025_completion_uninstall_bash.t \
    completion/t/0026_completion_uninstall_zsh.t \
    decoder/t/0004_tab_separated_colour_introducers.t \
    decoder/t/0005_oversized_dcs_geometry.t \
    dither/t/0001_roundtrip_dither_gamma.t \
    dither/t/0002_inspection_with_diffusion.t \
    dither/t/0003_ascii_ppm_inspection_diffusion.t \
    dither/t/0004_ascii_pbm_atkinson_inspection.t \
    dither/t/0005_cropping_with_x_dither.t \
    dither/t/0006_cropping_with_alternate_dither.t \
    dither/t/0007_sixel_inspection_alternate_dither.t \
    dither/t/0008_sixel_inspection_x_dither.t \
    dither/t/0009_lanczos2_without_diffusion.t \
    dither/t/0010_positional_8bit_dither.t \
    dither/t/0011_positional_float32_dither.t \
    dither/t/0012_varcoeff_8bit_dither.t \
    dither/t/0013_varcoeff_float32_dither.t \
    dither/t/0014_varcoeff_8bit_serpentine_lsqa.t \
    dither/t/0015_varcoeff_8bit_carry_lsqa.t \
    dither/t/0016_varcoeff_8bit_carry_serpentine_lsqa.t \
    dither/t/0017_varcoeff_float32_carry_lsqa.t \
    dither/t/0018_varcoeff_float32_carry_serpentine_lsqa.t \
    dither/t/0019_varcoeff_float32_oklab_lsqa.t \
    dither/t/0020_positional_8bit_x_dither_lsqa.t \
    dither/t/0021_positional_8bit_no_diffuse_lsqa.t \
    dither/t/0022_positional_float32_a_dither_lsqa.t \
    dither/t/0023_positional_float32_no_diffuse_lsqa.t \
    dither/t/0024_fixed_8bit_fs_carry_lsqa.t \
    dither/t/0025_fixed_8bit_fs_carry_serpentine_lsqa.t \
    dither/t/0026_fixed_8bit_atkinson_carry_lsqa.t \
    dither/t/0027_fixed_8bit_jajuni_carry_lsqa.t \
    dither/t/0028_fixed_8bit_stucki_carry_lsqa.t \
    dither/t/0029_fixed_8bit_burkes_carry_lsqa.t \
    dither/t/0030_fixed_8bit_sierra1_lsqa.t \
    dither/t/0031_fixed_8bit_sierra1_carry_lsqa.t \
    dither/t/0032_fixed_8bit_sierra2_lsqa.t \
    dither/t/0033_fixed_8bit_sierra2_carry_lsqa.t \
    dither/t/0034_fixed_8bit_sierra3_lsqa.t \
    dither/t/0035_fixed_8bit_sierra3_carry_lsqa.t \
    dither/t/0036_fixed_float32_fs_lsqa.t \
    dither/t/0037_fixed_float32_fs_serpentine_lsqa.t \
    dither/t/0038_fixed_float32_atkinson_lsqa.t \
    dither/t/0039_fixed_float32_jajuni_lsqa.t \
    dither/t/0040_fixed_float32_stucki_lsqa.t \
    dither/t/0041_fixed_float32_burkes_lsqa.t \
    dither/t/0042_fixed_float32_sierra1_lsqa.t \
    dither/t/0043_fixed_float32_sierra2_lsqa.t \
    dither/t/0044_fixed_float32_sierra3_lsqa.t \
    dither/t/0045_positional_8bit_bluenoise_gamma_lsqa.t \
    dither/t/0046_positional_float32_bluenoise_gamma_lsqa.t \
    dither/t/0047_positional_float32_bluenoise_oklab_lsqa.t \
    dither/t/0048_positional_float32_bluenoise_linear_lsqa.t \
    dither/t/0049_positional_float32_bluenoise_cielab_lsqa.t \
    dither/t/0050_positional_float32_bluenoise_din99d_lsqa.t \
    dither/t/0051_positional_8bit_a_dither_gamma_lsqa.t \
    dither/t/0052_positional_float32_a_dither_gamma_lsqa.t \
    dither/t/0053_positional_float32_a_dither_oklab_lsqa.t \
    dither/t/0054_positional_float32_a_dither_linear_lsqa.t \
    dither/t/0055_positional_float32_a_dither_cielab_lsqa.t \
    dither/t/0056_positional_float32_a_dither_din99d_lsqa.t \
    dither/t/0057_positional_8bit_x_dither_gamma_lsqa.t \
    dither/t/0058_positional_float32_x_dither_gamma_lsqa.t \
    dither/t/0059_positional_float32_x_dither_oklab_lsqa.t \
    dither/t/0060_positional_float32_x_dither_linear_lsqa.t \
    dither/t/0061_positional_float32_x_dither_cielab_lsqa.t \
    dither/t/0062_positional_float32_x_dither_din99d_lsqa.t \
    docs/t/0001_help_vs_man.t \
    docs/t/0002_man_vs_bash_completion.t \
    docs/t/0003_bash_vs_zsh_completion.t \
    docs/t/0004_envvars_vs_help.t \
    filter/t/0001_filter_clip.t \
    filter/t/0002_filter_sample.t \
    filter/t/0003_filter_resize.t \
    filter/t/0004_filter_colors.t \
    filter/t/0005_filter_lookup.t \
    filter/t/0006_filter_final_merge.t \
    filter/t/0007_filter_vpte.t \
    filter/t/0008_filter_load.t \
    filter/t/0009_filter_dither.t \
    filter/t/0010_filter_encode.t \
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
    gdk-pixbuf-loader/t/0001_gdk_pixbuf_loader.t \
    gdk-pixbuf-loader/t/0002_incremental_load.t \
    gdk-pixbuf-loader/t/0003_corrupt_data.t \
    gdk-pixbuf-loader/t/0004_propagate_error.t \
    gdk-pixbuf-loader/t/0005_context_free.t \
    gdk-pixbuf-loader/t/0006_loader_module.t \
    gdk-pixbuf-loader/t/0007_loader_plugin_integration.t \
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
    geometry/t/0015_resample_nearest_downscale_80pct_lsqa.t \
    geometry/t/0016_resample_gaussian_downscale_80pct_lsqa.t \
    geometry/t/0017_resample_hanning_downscale_80pct_lsqa.t \
    geometry/t/0018_resample_hamming_downscale_80pct_lsqa.t \
    geometry/t/0019_resample_bilinear_downscale_80pct_lsqa.t \
    geometry/t/0020_resample_welsh_downscale_80pct_lsqa.t \
    geometry/t/0021_resample_bicubic_downscale_80pct_lsqa.t \
    geometry/t/0022_resample_lanczos2_downscale_80pct_lsqa.t \
    geometry/t/0023_resample_lanczos3_downscale_80pct_lsqa.t \
    geometry/t/0024_resample_lanczos4_downscale_80pct_lsqa.t \
    geometry/t/0025_resample_nearest_upscale_120pct_lsqa.t \
    geometry/t/0026_resample_gaussian_upscale_120pct_lsqa.t \
    geometry/t/0027_resample_hanning_upscale_120pct_lsqa.t \
    geometry/t/0028_resample_hamming_upscale_120pct_lsqa.t \
    geometry/t/0029_resample_bilinear_upscale_120pct_lsqa.t \
    geometry/t/0030_resample_welsh_upscale_120pct_lsqa.t \
    geometry/t/0031_resample_bicubic_upscale_120pct_lsqa.t \
    geometry/t/0032_resample_lanczos2_upscale_120pct_lsqa.t \
    geometry/t/0033_resample_lanczos3_upscale_120pct_lsqa.t \
    geometry/t/0034_resample_lanczos4_upscale_120pct_lsqa.t \
    geometry/t/0035_resize_height_32_lsqa.t \
    geometry/t/0036_resize_height_32px_lsqa.t \
    geometry/t/0037_resize_height_80pct_lsqa.t \
    geometry/t/0038_resize_height_100_lsqa.t \
    geometry/t/0039_resize_height_100px_lsqa.t \
    geometry/t/0040_resize_height_120pct_lsqa.t \
    loader/t/0001_progressive_jpeg.t \
    loader/t/0002_pngsuite_basic_convert.t \
    loader/t/0003_pngsuite_basic_width32.t \
    loader/t/0004_pngsuite_basic_cropped.t \
    loader/t/0005_pngsuite_background_convert.t \
    loader/t/0006_pngsuite_background_white.t \
    loader/t/0007_pngsuite_background_width32_white.t \
    loader/t/0008_loader_coregraphics_pixelformat.t \
    loader/t/0009_loader_wic_pixelformat.t \
    loader/t/0010_loader_gdk_pixbuf_pixelformat.t \
    loader/t/0011_loader_gd_pixelformat.t \
    loader/t/0012_loader_libpng_pixelformat.t \
    loader/t/0013_loader_libjpeg_pixelformat.t \
    loader/t/0014_loader_builtin_pixelformat.t \
    loader/t/0015_loader_quicklook_pixelformat.t \
    loader/t/0016_loader_gnome_thumbnailer_pixelformat.t \
    loader/t/0017_loader_libwebp_pixelformat.t \
    loader/t/0018_loader_builtin_psd.t \
    loader/t/0019_loader_builtin_hdr.t \
    loader/t/0020_loader_builtin_pic.t \
    mapfile/t/0001_mapfile_export_act.t \
    mapfile/t/0002_mapfile_export_pal_default.t \
    mapfile/t/0003_mapfile_export_pal_stdout.t \
    mapfile/t/0004_mapfile_export_riff.t \
    mapfile/t/0005_mapfile_export_gpl.t \
    mapfile/t/0006_mapfile_import_gpl_prefixed.t \
    mapfile/t/0007_mapfile_import_act_extension.t \
    mapfile/t/0008_mapfile_import_riff_explicit.t \
    mapfile/t/0009_mapfile_import_stdin.t \
    network/t/0001_curl_invalid_file_url.t \
    network/t/0002_curl_invalid_https_url.t \
    network/t/0003_curl_file_scheme.t \
    network/t/0004_curl_self_signed_blocked.t \
    network/t/0005_curl_self_signed_with_k.t \
    output/t/0001_vt340_colour_sequences.t \
    output/t/0002_vt340_monochrome_sequences.t \
    output/t/0003_direct_sixel_encode.t \
    output/t/0004_sixel_metadata_inspection.t \
    output/t/0005_monochrome_frame_trailer.t \
    output/t/0006_two_pass_sixel_conversion.t \
    packaging/t/0001_python_wheel_install.t \
    packaging/t/0002_python_wheel_roundtrip.t \
    palette/t/0001_kmeans_init_default_unset.t \
    palette/t/0002_kmeans_init_auto.t \
    palette/t/0003_kmeans_init_unknown.t \
    palette/t/0004_kmeans_init_pca.t \
    palette/t/0005_kmeans_init_cache.t \
    palette/t/0006_palette_fallback_conversion.t \
    palette/t/0007_palette_fallback_output.t \
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
    palette/t/0025_lookup_policy_vpte_float32.t \
    palette/t/0026_merge_kmeans_float32_lsqa.t \
    palette/t/0027_merge_heckbert_float32_lsqa.t \
    palette/t/0028_merge_kmeans_8bit_lsqa.t \
    palette/t/0029_merge_heckbert_8bit_lsqa.t \
    palette/t/0030_snap_kmeans_float32_lsqa.t \
    palette/t/0031_snap_heckbert_float32_lsqa.t \
    palette/t/0032_snap_kmeans_8bit_lsqa.t \
    palette/t/0033_snap_heckbert_8bit_lsqa.t \
    palette/t/0034_lookup_policy_1d_eytzinger_8bit.t \
    palette/t/0035_lookup_policy_1d_eytzinger_float32.t \
    palette/t/0036_lookup_policy_1d_eytzinger_float32_gamma_lsqa.t \
    palette/t/0037_lookup_policy_1d_eytzinger_float32_linear_lsqa.t \
    palette/t/0038_lookup_policy_1d_eytzinger_float32_oklab_lsqa.t \
    palette/t/0039_lookup_policy_1d_eytzinger_float32_cielab_lsqa.t \
    palette/t/0040_lookup_policy_1d_eytzinger_float32_din99d_lsqa.t \
    palette/t/0041_lookup_policy_vpte_8bit_gamma_lsqa.t \
    palette/t/0042_lookup_policy_vpte_float32_gamma_lsqa.t \
    palette/t/0043_lookup_policy_vpte_float32_linear_lsqa.t \
    palette/t/0044_lookup_policy_vpte_float32_oklab_lsqa.t \
    palette/t/0045_lookup_policy_vpte_float32_cielab_lsqa.t \
    palette/t/0046_lookup_policy_vpte_float32_din99d_lsqa.t \
    palette/t/0047_colorspace_rgb_cielab_lsqa.t \
    palette/t/0048_colorspace_hls_oklab_lsqa.t \
    palette/t/0049_colorspace_hls_vs_rgb_lsqa.t \
    palette/t/0050_palette_map_png_rgb.t \
    palette/t/0051_palette_map_png_pal1.t \
    palette/t/0052_palette_map_png_pal2.t \
    palette/t/0053_palette_map_png_pal4.t \
    palette/t/0054_palette_map_png_pal8.t \
    palette/t/0055_palette_map_png_g1.t \
    palette/t/0056_palette_map_png_g2.t \
    palette/t/0057_palette_map_png_g4.t \
    palette/t/0058_palette_map_png_g8.t \
    palette/t/0059_clustering_colorspace_gamma_lsqa.t \
    palette/t/0060_clustering_colorspace_linear_lsqa.t \
    palette/t/0061_clustering_colorspace_oklab_lsqa.t \
    palette/t/0062_clustering_colorspace_cielab_lsqa.t \
    palette/t/0063_clustering_colorspace_din99d_lsqa.t \
    palette/t/0064_clustering_gamma_working_gamma_lsqa.t \
    palette/t/0065_clustering_gamma_working_linear_lsqa.t \
    palette/t/0066_clustering_gamma_working_oklab_lsqa.t \
    palette/t/0067_clustering_gamma_working_cielab_lsqa.t \
    palette/t/0068_clustering_gamma_working_din99d_lsqa.t \
    palette/t/0069_clustering_linear_working_gamma_lsqa.t \
    palette/t/0070_clustering_linear_working_linear_lsqa.t \
    palette/t/0071_clustering_linear_working_oklab_lsqa.t \
    palette/t/0072_clustering_linear_working_cielab_lsqa.t \
    palette/t/0073_clustering_linear_working_din99d_lsqa.t \
    palette/t/0074_clustering_oklab_working_gamma_lsqa.t \
    palette/t/0075_clustering_oklab_working_linear_lsqa.t \
    palette/t/0076_clustering_oklab_working_oklab_lsqa.t \
    palette/t/0077_clustering_oklab_working_cielab_lsqa.t \
    palette/t/0078_clustering_oklab_working_din99d_lsqa.t \
    palette/t/0079_clustering_cielab_working_gamma_lsqa.t \
    palette/t/0080_clustering_cielab_working_linear_lsqa.t \
    palette/t/0081_clustering_cielab_working_oklab_lsqa.t \
    palette/t/0082_clustering_cielab_working_cielab_lsqa.t \
    palette/t/0083_clustering_cielab_working_din99d_lsqa.t \
    palette/t/0084_clustering_din99d_working_gamma_lsqa.t \
    palette/t/0085_clustering_din99d_working_linear_lsqa.t \
    palette/t/0086_clustering_din99d_working_oklab_lsqa.t \
    palette/t/0087_clustering_din99d_working_cielab_lsqa.t \
    palette/t/0088_clustering_din99d_working_din99d_lsqa.t \
    pipeline/t/0001_pipeline_planner_baseline_run.t \
    pipeline/t/0002_pipeline_planner_baseline_summary.t \
    pipeline/t/0003_pipeline_planner_baseline_threads.t \
    pipeline/t/0004_pipeline_planner_overrides_run.t \
    pipeline/t/0005_pipeline_planner_overrides_summary.t \
    pipeline/t/0006_pipeline_planner_overrides_threads.t \
    poc/t/0001_issue167_empty_bg.t \
    poc/t/0002_issue167_height.t \
    poc/t/0003_issue166_width.t \
    poc/t/0004_issue200_heap_overflow.t \
    poc/t/0005_issue131_img2sixel_gif.t \
    poc/t/0006_issue166_height.t \
    poc/t/0007_issue167_height_option_first.t \
    poc/t/0008_issue200_heap_overflow_reject.t \
    poc/t/0009_issue207_resize_poc.t \
    poc/t/0010_issue51_stbi_1561_poc.t \
    poc/t/0011_issue73_stbi_1561_poc.t \
    probe/t/0001_probe_parse.t \
    python/t/0001_python_bindings.t \
    python/t/0002_python_api_formats.t \
    python/t/0003_python_api_options.t \
    python/t/0004_python_api_errors.t \
    python/t/0005_python_resource_management.t \
    regression/t/0005_lsqa_format_grayscale.t \
    regression/t/0006_lsqa_format_palette.t \
    regression/t/0007_lsqa_format_rgb.t \
    regression/t/0015_lsqa_repeat_palette_variance.t \
    regression/t/0016_lsqa_corrupted_metadata_noise.t \
    regression/t/0017_lsqa_corrupted_truncated_png.t \
    regression/t/0018_lsqa_format_tga_type1_pal4.t \
    regression/t/0019_lsqa_format_tga_type1_pal8.t \
    regression/t/0020_lsqa_format_tga_type2_rgb.t \
    regression/t/0021_lsqa_format_tga_type3_gray.t \
    regression/t/0022_lsqa_format_tga_type9_pal4.t \
    regression/t/0023_lsqa_format_tga_type9_pal8.t \
    regression/t/0024_lsqa_format_tga_type10_rgb.t \
    regression/t/0025_lsqa_format_tga_type11_gray.t \
    regression/t/0026_lsqa_roundtrip_rgba_small.t \
    regression/t/0027_lsqa_format_tiff_rgb_small.t \
    regression/t/0028_lsqa_format_tiff_palette_small.t \
    regression/t/0029_lsqa_format_tiff_grayscale.t \
    regression/t/0030_lsqa_format_tiff_rgba.t \
    regression/t/0031_lsqa_format_tiff_lzw.t \
    regression/t/0032_lsqa_format_tiff_tiled.t \
    sixel2png/t/0001_version.t \
    sixel2png/t/0002_help.t \
    sixel2png/t/0002_missing_args.t \
    sixel2png/t/0003_unknown_option.t \
    sixel2png/t/0004_invalid_decoder_value.t \
    sixel2png/t/0005_default_output.t

SERIAL_TESTS = \
    aborttrace/t/0001_img2sixel_aborttrace.t

COPY_TESTS = $(TESTS)
check_PROGRAMS = test_runner

TEST_EXTENSIONS = .t

HELPER_FILES = \
    _lib/sh/common.sh \
    docs/list_envvars.sh \
    lib/sh/conversion/common.sh \
    lib/sh/common/tap.sh \
    lib/sh/cli-core/cli_core_common.sh \
    lib/sh/mapfile/mapfile_common.sh \
    lib/sh/pipeline/pipeline_planner_common.sh \
    lib/sh/packaging/python_wheel_common.sh \
    lib/sh/palette/kmeans_init_common.sh \
    lib/sh/lsqa/lsqa_common.sh \
    lib/sh/loader/pngsuite_common.sh \
    lib/sh/python/common.sh \
    decoder/t/dcs_arguments_common.sh

ARTIFACT_ROOT = $(abs_builddir)/_artifacts
AM_TESTS_ENVIRONMENT = ARTIFACT_ROOT='$(ARTIFACT_ROOT)'; export ARTIFACT_ROOT;
AM_TESTS_ENVIRONMENT += TOP_SRCDIR='$(abs_top_srcdir)'; export TOP_SRCDIR;
AM_TESTS_ENVIRONMENT += TOP_BUILDDIR='$(abs_top_builddir)'; export TOP_BUILDDIR;
AM_TESTS_ENVIRONMENT += SIXEL_TSAN_BUILD='@SIXEL_TSAN_BUILD@'; \
       export SIXEL_TSAN_BUILD;
AM_TESTS_ENVIRONMENT += CROSS_COMPILING='@cross_compiling@'; \
       export CROSS_COMPILING;
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
    aborttrace/0001_img2sixel_aborttrace.c \
    cli/0029_cli_token_is_known_option.c \
    cli/0030_cli_option_requires_argument.c \
    cli/0031_cli_guard_missing_argument.c \
    filter/0001_filter_clip.c \
    filter/0002_filter_sample.c \
    filter/0003_filter_resize.c \
    filter/0004_filter_colors.c \
    filter/0005_filter_lookup.c \
    filter/0006_filter_final_merge.c \
    filter/0007_filter_vpte.c \
    filter/0008_filter_load.c \
    filter/0009_filter_dither.c \
    filter/0010_filter_encode.c \
    filter/filter_test_common.h \
    loader/0008_loader_coregraphics_pixelformat.c \
    loader/0009_loader_wic_pixelformat.c \
    loader/0010_loader_gdk_pixbuf_pixelformat.c \
    loader/0011_loader_gd_pixelformat.c \
    loader/0012_loader_libpng_pixelformat.c \
    loader/0013_loader_libjpeg_pixelformat.c \
    loader/0014_loader_builtin_pixelformat.c \
    loader/0015_loader_quicklook_pixelformat.c \
    loader/0016_loader_gnome_thumbnailer_pixelformat.c \
    loader/0017_loader_libwebp_pixelformat.c \
    loader/pixelformat_test_common.h \
    palette/0001_kmeans_init.c \
    probe/0001_probe_parse.c \
    gdk-pixbuf-loader/0001_gdk_pixbuf_loader.c \
    gdk-pixbuf-loader/0002_incremental_load.c \
    gdk-pixbuf-loader/0003_corrupt_data.c \
    gdk-pixbuf-loader/0004_propagate_error.c \
    gdk-pixbuf-loader/0005_context_free.c \
    gdk-pixbuf-loader/0006_loader_module.c \
    gdk-pixbuf-loader/0007_loader_plugin_integration.c \
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
    data/inputs/snake_64.png \
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
    issue/166/poc \
    issue/167/poc \
    issue/200/POC_img2sixel_heap_buffer_overflow \
    issue/207/poc \
    issue/libsixel-libsixel/51/stbi_1561_poc.bin \
    issue/libsixel-libsixel/73/stbi_1561_poc.bin \
    issue/131/2020-01-30-img2sixel.gif \
    issue/136/poc \
    network/certs/server.crt \
    network/certs/server.key

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
