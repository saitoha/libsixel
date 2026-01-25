sh = find_program('sh')
artifact_root = meson.current_build_dir() / '_artifacts'
env = environment()
env.set('ARTIFACT_ROOT', artifact_root)
env.set('TOP_SRCDIR', meson.project_source_root())
env.set('TOP_BUILDDIR', meson.project_build_root())
env.set('MESON_SOURCE_ROOT', meson.project_source_root())
env.set('MESON_BUILD_ROOT', meson.project_build_root())
cross_compiling = 'no'
if meson.is_cross_build()
  cross_compiling = 'yes'
endif
env.set('CROSS_COMPILING', cross_compiling)
# Ensure Python TAP tests can locate the freshly built shared library when
# Meson is invoked without a prior `meson compile`. The binding smoke test
# relies on LIBSIXEL_LIBDIR to point at the build artifact directory.
env.set('LIBSIXEL_LIBDIR', meson.project_build_root() / 'src')

test_runner_sources = [
  'test_runner.c',
  'cli/0029_cli_token_is_known_option.c',
  'cli/0030_cli_option_requires_argument.c',
  'cli/0031_cli_guard_missing_argument.c',
  'filter/0001_filter_clip.c',
  'filter/0002_filter_sample.c',
  'filter/0003_filter_resize.c',
  'filter/0004_filter_colors.c',
  'filter/0005_filter_lookup.c',
  'filter/0006_filter_final_merge.c',
  'filter/0007_filter_vpte.c',
  'filter/0008_filter_load.c',
  'filter/0009_filter_dither.c',
  'filter/0010_filter_encode.c',
  'loader/0008_loader_coregraphics_pixelformat.c',
  'loader/0009_loader_wic_pixelformat.c',
  'loader/0010_loader_gdk_pixbuf_pixelformat.c',
  'loader/0011_loader_gd_pixelformat.c',
  'loader/0012_loader_libpng_pixelformat.c',
  'loader/0013_loader_libjpeg_pixelformat.c',
  'loader/0014_loader_builtin_pixelformat.c',
  'loader/0015_loader_quicklook_pixelformat.c',
  'loader/0016_loader_gnome_thumbnailer_pixelformat.c',
  'palette/0001_kmeans_init.c',
  'probe/0001_probe_parse.c',
  '../converters/cli.c',
]
test_runner_deps = [libsixel_dep]
test_runner_c_args = []

if gdk_pixbuf_loader_deps.length() > 0
  test_runner_sources += [
    'gdk-pixbuf-loader/0001_gdk_pixbuf_loader.c',
    'gdk-pixbuf-loader/0002_incremental_load.c',
    'gdk-pixbuf-loader/0003_corrupt_data.c',
    'gdk-pixbuf-loader/0004_propagate_error.c',
    'gdk-pixbuf-loader/0005_context_free.c',
    'gdk-pixbuf-loader/0006_loader_module.c',
    'gdk-pixbuf-loader/0007_loader_plugin_integration.c',
    '../gdk-pixbuf-loader/sixel-pixbuf-loader.c',
  ]
  test_runner_deps += gdk_pixbuf_loader_deps
  test_runner_c_args += ['-DGDK_PIXBUF_ENABLE_BACKEND',
                         '-DSIXEL_PIXBUF_LOADER_TESTING',
                         '-DSIXEL_ENABLE_GDK_PIXBUF_LOADER_TESTS']
endif

test_runner_inc = [
  project_inc,
  include_directories('..'),
]

test_runner_exe = executable('test_runner', test_runner_sources,
  include_directories: test_runner_inc,
  dependencies: test_runner_deps,
  c_args: test_runner_c_args,
  install: false)

if host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
  # Ensure Windows-style runners (including MSYS2) locate the freshly built
  # DLLs when executing the converter TAP suite. Without these PATH entries
  # the tests fail with "cannot open shared object file" errors because
  # libsixel-1.dll or msys-sixel-1.dll is not discoverable beside the
  # converter executables.
  dll_paths = [meson.project_build_root() / 'src',
               meson.project_build_root() / 'converters']
  foreach dll_path : dll_paths
    env.prepend('PATH', dll_path)
  endforeach
endif

issue_inputs = files('issue/166/poc', 'issue/167/poc',
  'issue/200/POC_img2sixel_heap_buffer_overflow')
meson.add_dist_script('sh', '-c', 'true', issue_inputs)

cli_tests = ['cli-core/t/0001_help.t',
             'cli-core/t/0002_version.t',
             'cli-core/t/0003_stdin.t',
             'cli-core/t/0004_basic_missing_input.t',
             'cli-core/t/0005_basic_help_option.t',
             'cli-core/t/0006_basic_version_option.t',
             'cli-core/t/0007_basic_stdin_snake.t',
             'cli-core/t/0008_basic_stdin_map8.t',
             'cli-core/t/0009_basic_stdin_stdout_map64.t',
             'cli-core/t/0010_basic_file_arguments_snake.t',
             'cli-core/t/0011_basic_direct_output.t',
             'cli-core/t/0012_basic_direct_with_dequantize_conflict.t',
             'cli-core/t/0013_basic_png_prefix_path.t',
             'cli-core/t/0014_basic_png_stdout.t',
             'cli-core/t/0015_basic_png_prefix_missing_target.t',
             'cli-core/t/0016_basic_dequantize_unique_prefix.t',
             'cli-core/t/0017_basic_parallel_indexed.t',
             'cli-core/t/0018_basic_parallel_direct.t',
             'cli-core/t/0019_basic_parallel_cli_override.t',
             'cli-core/t/0020_basic_invalid_threads_token.t',
             'completion/t/0021_completion_show_bash.t',
             'completion/t/0022_completion_show_zsh.t',
             'completion/t/0023_completion_install_bash.t',
             'completion/t/0024_completion_install_zsh.t',
             'completion/t/0025_completion_uninstall_bash.t',
             'completion/t/0026_completion_uninstall_zsh.t',
             'cli-option-invalid/t/0001a_invalid_option_arguments_unknown_dither.t',
             'cli-option-invalid/t/0001b_invalid_option_arguments_unknown_resize_filter.t',
             'cli-option-invalid/t/0001c_invalid_option_arguments_unknown_scaling_mode.t',
             'cli-option-invalid/t/0001d_invalid_option_arguments_unknown_tone_adjustment.t',
             'cli-option-invalid/t/0001e_invalid_option_arguments_invalid_width.t',
             'cli-option-invalid/t/0001f_invalid_option_arguments_invalid_height.t',
             'cli-option-invalid/t/0001g_invalid_option_arguments_invalid_format.t',
             'cli-option-invalid/t/0001h_invalid_option_arguments_invalid_quality.t',
             'cli-option-invalid/t/0001i_invalid_option_arguments_invalid_layout.t',
             'cli-option-invalid/t/0001j_invalid_option_arguments_invalid_bpp.t',
             'cli-option-invalid/t/0001k_invalid_option_arguments_invalid_encoder_tweak.t',
             'cli-option-invalid/t/0001l_invalid_option_arguments_invalid_background_colour.t',
             'cli-option-invalid/t/0001m_invalid_option_arguments_missing_background_component.t',
             'cli-option-invalid/t/0001n_invalid_option_arguments_overlong_background.t',
             'cli-option-invalid/t/0001o_invalid_option_arguments_malformed_hex_background.t',
             'cli-option-invalid/t/0001p_invalid_option_arguments_unknown_named_colour.t',
             'cli-option-invalid/t/0001q_invalid_option_arguments_incomplete_rgb_background.t',
             'cli-option-invalid/t/0001r_invalid_option_arguments_unsupported_legacy_width.t',
             'cli-option-invalid/t/0001s_invalid_option_arguments_missing_palette_file.t',
             'cli-option-invalid/t/0001t_invalid_option_arguments_invalid_colour_space_index.t',
             'cli-option-invalid/t/0002_invalid_option_arguments_invalid_width.t',
             'cli-option-invalid/t/0003_invalid_option_arguments_invalid_height.t',
             'cli-option-invalid/t/0004_invalid_option_arguments_invalid_format.t',
             'cli-option-invalid/t/0005_invalid_option_arguments_invalid_quality.t',
             'cli-option-invalid/t/0006_invalid_option_arguments_invalid_layout.t',
             'cli-option-invalid/t/0007_invalid_option_arguments_invalid_bpp.t',
             'cli-option-invalid/t/0008_invalid_option_arguments_invalid_encoder_tweak.t',
             'cli-option-invalid/t/0009_invalid_option_arguments_invalid_background_colour.t',
             'cli-option-invalid/t/0010_invalid_option_arguments_missing_background_component.t',
             'cli-option-invalid/t/0011_invalid_option_arguments_overlong_background.t',
             'cli-option-invalid/t/0012_invalid_option_arguments_malformed_hex_background.t',
             'cli-option-invalid/t/0013_invalid_option_arguments_unknown_named_colour.t',
             'cli-option-invalid/t/0014_invalid_option_arguments_incomplete_rgb_background.t',
             'cli-option-invalid/t/0015_invalid_option_arguments_unsupported_legacy_width.t',
             'cli-option-invalid/t/0016_invalid_option_arguments_missing_palette_file.t',
             'cli-option-invalid/t/0017_invalid_option_arguments_invalid_colour_space_index.t',
             'cli-option-invalid/t/0018_invalid_option_arguments_unknown_dither.t',
             'cli-option-invalid/t/0019_invalid_option_arguments_unknown_resize_filter.t',
             'cli-option-invalid/t/0020_invalid_option_arguments_unknown_scaling_mode.t',
             'cli-option-invalid/t/0021_invalid_option_arguments_unknown_tone_adjustment.t',
             'cli-option-invalid/t/0022a_invalid_option_combinations_palette_and_encode.t',
             'cli-option-invalid/t/0022b_invalid_option_combinations_inspect_with_palette.t',
             'cli-option-invalid/t/0022c_invalid_option_combinations_palette_with_terminal_preset.t',
             'cli-option-invalid/t/0022d_invalid_option_combinations_eight_bit_with_palette_dump.t',
             'cli-option-invalid/t/0023_invalid_option_combinations_palette_and_encode.t',
             'cli-option-invalid/t/0024_invalid_option_combinations_inspect_with_palette.t',
             'cli-option-invalid/t/0025_invalid_option_combinations_palette_with_terminal_preset.t',
             'cli-option-invalid/t/0026_invalid_option_combinations_eight_bit_with_palette_dump.t',
             'cli-option-matching/t/0001_option_matching_prefix_unique.t',
             'cli-option-matching/t/0002_option_matching_prefix_ambiguous.t',
             'cli-option-matching/t/0003_option_matching_diffusion_correction.t',
             'cli-option-matching/t/0004_option_matching_distance1_multi.t',
             'cli-option-matching/t/0005_option_matching_distance2.t',
             'cli-option-matching/t/0006_option_matching_distance3.t',
             'cli-argument-shift/t/0001_invalid_positional.t',
             'cli-argument-shift/t/0002_argument_shift_missing_mapfile.t',
             'cli-argument-shift/t/0003_argument_shift_outfile_named_like_option.t',
             'cli-argument-shift/t/0004_argument_shift_assessment_named_like_option.t',
             'cli-argument-shift/t/0005_cli_token_is_known_option.t',
             'cli-argument-shift/t/0006_cli_option_requires_argument.t',
             'cli-argument-shift/t/0007_cli_guard_missing_argument.t',
             'palette/t/0008_select_color_ambiguous_prefix_rejected.t',
             'palette/t/0009_select_color_unique_prefix_accepted.t',
             'palette/t/0010_explicit_dimensions_with_palette.t',
             'palette/t/0011_fast_encoder_external_palette.t',
             'palette/t/0012_palette16_conversion.t',
             'palette/t/0013_xterm_palette_override_repetition.t',
             'palette/t/0014_tiff_palette_filters.t',
             'palette/t/0015_grayscale_png_with_palette.t',
             'palette/t/0016_apply_grayscale_palette_to_colour_png.t',
             'palette/t/0017_stacked_palette_files.t',
             'palette/t/0018_xterm16_palette_preset.t',
             'palette/t/0019_jpeg_palette_welsh.t',
             'palette/t/0020_ppm_palette16_conversion.t',
             'palette/t/0021_lanczos2_palette_limit.t',
             'palette/t/0022_lanczos3_two_colour_palette.t',
             'palette/t/0023_lanczos4_palette_dump.t',
             'palette/t/0024_interlaced_palette_dump.t',
             'palette/t/0025_lookup_policy_vpte_float32.t',
             'palette/t/0026_merge_kmeans_float32_lsqa.t',
             'palette/t/0027_merge_heckbert_float32_lsqa.t',
             'palette/t/0028_merge_kmeans_8bit_lsqa.t',
             'palette/t/0029_merge_heckbert_8bit_lsqa.t',
             'palette/t/0030_snap_kmeans_float32_lsqa.t',
             'palette/t/0031_snap_heckbert_float32_lsqa.t',
            'palette/t/0032_snap_kmeans_8bit_lsqa.t',
            'palette/t/0033_snap_heckbert_8bit_lsqa.t',
            'palette/t/0034_lookup_policy_1d_eytzinger_8bit.t',
            'palette/t/0035_lookup_policy_1d_eytzinger_float32.t',
            'palette/t/0036_lookup_policy_1d_eytzinger_float32_gamma_lsqa.t',
            'palette/t/0037_lookup_policy_1d_eytzinger_float32_linear_lsqa.t',
            'palette/t/0038_lookup_policy_1d_eytzinger_float32_oklab_lsqa.t',
            'palette/t/0039_lookup_policy_1d_eytzinger_float32_cielab_lsqa.t',
            'palette/t/0040_lookup_policy_1d_eytzinger_float32_din99d_lsqa.t',
            'palette/t/0041_lookup_policy_vpte_8bit_gamma_lsqa.t',
            'palette/t/0042_lookup_policy_vpte_float32_gamma_lsqa.t',
            'palette/t/0043_lookup_policy_vpte_float32_linear_lsqa.t',
            'palette/t/0044_lookup_policy_vpte_float32_oklab_lsqa.t',
            'palette/t/0045_lookup_policy_vpte_float32_cielab_lsqa.t',
            'palette/t/0046_lookup_policy_vpte_float32_din99d_lsqa.t',
            'dither/t/0001_roundtrip_dither_gamma.t',
             'dither/t/0002_inspection_with_diffusion.t',
             'dither/t/0003_ascii_ppm_inspection_diffusion.t',
             'dither/t/0004_ascii_pbm_atkinson_inspection.t',
             'dither/t/0005_cropping_with_x_dither.t',
             'dither/t/0006_cropping_with_alternate_dither.t',
             'dither/t/0007_sixel_inspection_alternate_dither.t',
             'dither/t/0008_sixel_inspection_x_dither.t',
             'dither/t/0009_lanczos2_without_diffusion.t',
             'dither/t/0010_positional_8bit_dither.t',
             'dither/t/0011_positional_float32_dither.t',
             'dither/t/0012_varcoeff_8bit_dither.t',
             'dither/t/0013_varcoeff_float32_dither.t',
             'dither/t/0014_varcoeff_8bit_serpentine_lsqa.t',
             'dither/t/0015_varcoeff_8bit_carry_lsqa.t',
             'dither/t/0016_varcoeff_8bit_carry_serpentine_lsqa.t',
             'dither/t/0017_varcoeff_float32_carry_lsqa.t',
             'dither/t/0018_varcoeff_float32_carry_serpentine_lsqa.t',
             'dither/t/0019_varcoeff_float32_oklab_use_lr_lsqa.t',
             'dither/t/0020_positional_8bit_x_dither_lsqa.t',
             'dither/t/0021_positional_8bit_no_diffuse_lsqa.t',
             'dither/t/0022_positional_float32_a_dither_lsqa.t',
             'dither/t/0023_positional_float32_no_diffuse_lsqa.t',
             'dither/t/0024_fixed_8bit_fs_carry_lsqa.t',
             'dither/t/0025_fixed_8bit_fs_carry_serpentine_lsqa.t',
             'dither/t/0026_fixed_8bit_atkinson_carry_lsqa.t',
             'dither/t/0027_fixed_8bit_jajuni_carry_lsqa.t',
             'dither/t/0028_fixed_8bit_stucki_carry_lsqa.t',
             'dither/t/0029_fixed_8bit_burkes_carry_lsqa.t',
             'dither/t/0030_fixed_8bit_sierra1_lsqa.t',
             'dither/t/0031_fixed_8bit_sierra1_carry_lsqa.t',
             'dither/t/0032_fixed_8bit_sierra2_lsqa.t',
             'dither/t/0033_fixed_8bit_sierra2_carry_lsqa.t',
             'dither/t/0034_fixed_8bit_sierra3_lsqa.t',
             'dither/t/0035_fixed_8bit_sierra3_carry_lsqa.t',
             'dither/t/0036_fixed_float32_fs_lsqa.t',
             'dither/t/0037_fixed_float32_fs_serpentine_lsqa.t',
             'dither/t/0038_fixed_float32_atkinson_lsqa.t',
             'dither/t/0039_fixed_float32_jajuni_lsqa.t',
             'dither/t/0040_fixed_float32_stucki_lsqa.t',
             'dither/t/0041_fixed_float32_burkes_lsqa.t',
             'dither/t/0042_fixed_float32_sierra1_lsqa.t',
             'dither/t/0043_fixed_float32_sierra2_lsqa.t',
             'dither/t/0044_fixed_float32_sierra3_lsqa.t',
             'geometry/t/0001_scaling_histogram_background.t',
             'geometry/t/0002_dcs_crc_width_scaling.t',
             'geometry/t/0003_dcs_crc_height_scaling.t',
             'geometry/t/0004_dcs_crc_height_scaling_auto_width.t',
             'geometry/t/0005_dcs_crc_height_auto_width_absolute.t',
             'geometry/t/0006_dcs_crc_height_absolute_width_percent.t',
             'geometry/t/0007_tga_scaling.t',
             'geometry/t/0008_indexed_png_scaling.t',
             'geometry/t/0009_sixel_resize_palette_limit.t',
             'geometry/t/0010_sixel_cropping_large_offsets.t',
             'geometry/t/0011_ppm_inspection_aggressive_scaling.t',
             'geometry/t/0012_resize_precision_mode_run.t',
             'geometry/t/0013_resize_precision_mode_scaler_input.t',
             'geometry/t/0014_resize_precision_mode_post_colorspace.t',
             'format/t/0001_prefixed_png_output_default.t',
             'format/t/0002_prefixed_png_output_explicit_path.t',
             'format/t/0003_filename_driven_png_header.t',
             'format/t/0004_gif_filters.t',
             'format/t/0005_pgm_encode_flag.t',
             'format/t/0006_gray1_output.t',
             'format/t/0007_gray2_output.t',
             'format/t/0008_gray4_output.t',
             'format/t/0009_gray8_output.t',
             'format/t/0010_png_inspection_color_space.t',
             'format/t/0011_ascii_pgm_auto_encoder_inspection.t',
             'format/t/0012_grayscale_png_conversion.t',
             'format/t/0013_grayscale_png_inspection.t',
             'format/t/0014_grayscale_jpeg_inspection.t',
             'format/t/0015_bmp_inspection_with_filters.t',
             'output/t/0001_vt340_colour_sequences.t',
             'output/t/0002_vt340_monochrome_sequences.t',
             'output/t/0003_direct_sixel_encode.t',
             'output/t/0004_sixel_metadata_inspection.t',
             'output/t/0005_monochrome_frame_trailer.t',
             'output/t/0006_two_pass_sixel_conversion.t',
             'cli-options/t/0001_long_option_forms.t',
            'decoder/t/0004_tab_separated_colour_introducers.t',
            'decoder/t/0005_oversized_dcs_geometry.t',
            'animation/t/0001_disable_update_mode.t',
            'animation/t/0002_static_frame_render.t',
            'animation/t/0003_update_and_static_frame.t',
            'animation/t/0004_split_sequence_atkinson.t',
             'loader/t/0001_progressive_jpeg.t',
            'loader/t/0002_pngsuite_basic_convert.t',
            'loader/t/0003_pngsuite_basic_width32.t',
            'loader/t/0004_pngsuite_basic_cropped.t',
            'loader/t/0005_pngsuite_background_convert.t',
            'loader/t/0006_pngsuite_background_white.t',
            'loader/t/0007_pngsuite_background_width32_white.t',
            'network/t/0001_curl_invalid_file_url.t',
            'network/t/0002_curl_invalid_https_url.t',
            'network/t/0003_curl_file_scheme.t',
            'network/t/0004_curl_self_signed_blocked.t',
            'network/t/0005_curl_self_signed_with_k.t',
             'regression/t/0001_issue167_empty_bg.t',
             'regression/t/0002_issue167_height.t',
             'regression/t/0003_issue166_width.t',
             'regression/t/0004_issue200_heap_overflow.t',
             'regression/t/0005_lsqa_format_grayscale.t',
             'regression/t/0006_lsqa_format_palette.t',
             'regression/t/0007_lsqa_format_rgb.t',
             'regression/t/0008_lsqa_format_rgba.t',
             'regression/t/0012_lsqa_resolution_quarter_square.t',
             'regression/t/0014_lsqa_resolution_tiny_square.t',
             'regression/t/0015_lsqa_repeat_palette_variance.t',
             'regression/t/0016_lsqa_corrupted_metadata_noise.t',
             'regression/t/0017_lsqa_corrupted_truncated_png.t',
             'regression/t/0018_lsqa_format_tga_type1_pal4.t',
             'regression/t/0019_lsqa_format_tga_type1_pal8.t',
             'regression/t/0020_lsqa_format_tga_type2_rgb.t',
             'regression/t/0021_lsqa_format_tga_type3_gray.t',
             'regression/t/0022_lsqa_format_tga_type9_pal4.t',
             'regression/t/0023_lsqa_format_tga_type9_pal8.t',
             'regression/t/0024_lsqa_format_tga_type10_rgb.t',
             'regression/t/0025_lsqa_format_tga_type11_gray.t',
             'docs/t/0001_help_vs_man.t',
             'docs/t/0002_man_vs_bash_completion.t',
             'docs/t/0003_bash_vs_zsh_completion.t',
             'docs/t/0004_envvars_vs_help.t',
             'assessment/t/0001_spool.t',
             'clipboard/t/0001_clipboard_basic.t',
             'packaging/t/0001_python_wheel_install.t',
             'packaging/t/0002_python_wheel_roundtrip.t',
             'mapfile/t/0001_mapfile_export_act.t',
             'mapfile/t/0002_mapfile_export_pal_default.t',
             'mapfile/t/0003_mapfile_export_pal_stdout.t',
             'mapfile/t/0004_mapfile_export_riff.t',
             'mapfile/t/0005_mapfile_export_gpl.t',
             'mapfile/t/0006_mapfile_import_gpl_prefixed.t',
             'mapfile/t/0007_mapfile_import_act_extension.t',
             'mapfile/t/0008_mapfile_import_riff_explicit.t',
             'mapfile/t/0009_mapfile_import_stdin.t',
            'pipeline/t/0001_pipeline_planner_baseline_run.t',
            'pipeline/t/0002_pipeline_planner_baseline_summary.t',
            'pipeline/t/0003_pipeline_planner_baseline_threads.t',
            'pipeline/t/0004_pipeline_planner_overrides_run.t',
            'pipeline/t/0005_pipeline_planner_overrides_summary.t',
            'pipeline/t/0006_pipeline_planner_overrides_threads.t',
             'sixel2png/t/0001_version.t',
             'sixel2png/t/0002_help.t',
             'sixel2png/t/0002_missing_args.t',
             'sixel2png/t/0003_unknown_option.t',
             'sixel2png/t/0004_invalid_decoder_value.t',
             'sixel2png/t/0005_default_output.t']

aborttrace_tests = [
  'aborttrace/t/0001_img2sixel_aborttrace.t',
  'aborttrace/t/0002_sixel2png_aborttrace.t',
]

cli_depends = {
  'regression/t/0005_lsqa_format_grayscale.t': [lsqa_bin],
  'regression/t/0006_lsqa_format_palette.t': [lsqa_bin],
  'regression/t/0007_lsqa_format_rgb.t': [lsqa_bin],
  'regression/t/0008_lsqa_format_rgba.t': [lsqa_bin],
  'regression/t/0012_lsqa_resolution_quarter_square.t': [lsqa_bin],
  'regression/t/0014_lsqa_resolution_tiny_square.t': [lsqa_bin],
  'regression/t/0015_lsqa_repeat_palette_variance.t': [lsqa_bin],
  'regression/t/0016_lsqa_corrupted_metadata_noise.t': [lsqa_bin],
  'regression/t/0017_lsqa_corrupted_truncated_png.t': [lsqa_bin],
  'palette/t/0026_merge_kmeans_float32_lsqa.t': [lsqa_bin],
  'palette/t/0027_merge_heckbert_float32_lsqa.t': [lsqa_bin],
  'palette/t/0028_merge_kmeans_8bit_lsqa.t': [lsqa_bin],
  'palette/t/0029_merge_heckbert_8bit_lsqa.t': [lsqa_bin],
  'palette/t/0030_snap_kmeans_float32_lsqa.t': [lsqa_bin],
  'palette/t/0031_snap_heckbert_float32_lsqa.t': [lsqa_bin],
  'palette/t/0032_snap_kmeans_8bit_lsqa.t': [lsqa_bin],
  'palette/t/0033_snap_heckbert_8bit_lsqa.t': [lsqa_bin],
  'palette/t/0036_lookup_policy_1d_eytzinger_float32_gamma_lsqa.t': [lsqa_bin],
  'palette/t/0037_lookup_policy_1d_eytzinger_float32_linear_lsqa.t': [lsqa_bin],
  'palette/t/0038_lookup_policy_1d_eytzinger_float32_oklab_lsqa.t': [lsqa_bin],
  'palette/t/0039_lookup_policy_1d_eytzinger_float32_cielab_lsqa.t': [lsqa_bin],
  'palette/t/0040_lookup_policy_1d_eytzinger_float32_din99d_lsqa.t': [lsqa_bin],
  'palette/t/0041_lookup_policy_vpte_8bit_gamma_lsqa.t': [lsqa_bin],
  'palette/t/0042_lookup_policy_vpte_float32_gamma_lsqa.t': [lsqa_bin],
  'palette/t/0043_lookup_policy_vpte_float32_linear_lsqa.t': [lsqa_bin],
  'palette/t/0044_lookup_policy_vpte_float32_oklab_lsqa.t': [lsqa_bin],
  'palette/t/0045_lookup_policy_vpte_float32_cielab_lsqa.t': [lsqa_bin],
  'palette/t/0046_lookup_policy_vpte_float32_din99d_lsqa.t': [lsqa_bin],
  'dither/t/0014_varcoeff_8bit_serpentine_lsqa.t': [lsqa_bin],
  'dither/t/0015_varcoeff_8bit_carry_lsqa.t': [lsqa_bin],
  'dither/t/0016_varcoeff_8bit_carry_serpentine_lsqa.t': [lsqa_bin],
  'dither/t/0017_varcoeff_float32_carry_lsqa.t': [lsqa_bin],
  'dither/t/0018_varcoeff_float32_carry_serpentine_lsqa.t': [lsqa_bin],
  'dither/t/0019_varcoeff_float32_oklab_use_lr_lsqa.t': [lsqa_bin],
  'dither/t/0020_positional_8bit_x_dither_lsqa.t': [lsqa_bin],
  'dither/t/0021_positional_8bit_no_diffuse_lsqa.t': [lsqa_bin],
  'dither/t/0022_positional_float32_a_dither_lsqa.t': [lsqa_bin],
  'dither/t/0023_positional_float32_no_diffuse_lsqa.t': [lsqa_bin],
  'dither/t/0024_fixed_8bit_fs_carry_lsqa.t': [lsqa_bin],
  'dither/t/0025_fixed_8bit_fs_carry_serpentine_lsqa.t': [lsqa_bin],
  'dither/t/0026_fixed_8bit_atkinson_carry_lsqa.t': [lsqa_bin],
  'dither/t/0027_fixed_8bit_jajuni_carry_lsqa.t': [lsqa_bin],
  'dither/t/0028_fixed_8bit_stucki_carry_lsqa.t': [lsqa_bin],
  'dither/t/0029_fixed_8bit_burkes_carry_lsqa.t': [lsqa_bin],
  'dither/t/0030_fixed_8bit_sierra1_lsqa.t': [lsqa_bin],
  'dither/t/0031_fixed_8bit_sierra1_carry_lsqa.t': [lsqa_bin],
  'dither/t/0032_fixed_8bit_sierra2_lsqa.t': [lsqa_bin],
  'dither/t/0033_fixed_8bit_sierra2_carry_lsqa.t': [lsqa_bin],
  'dither/t/0034_fixed_8bit_sierra3_lsqa.t': [lsqa_bin],
  'dither/t/0035_fixed_8bit_sierra3_carry_lsqa.t': [lsqa_bin],
  'dither/t/0036_fixed_float32_fs_lsqa.t': [lsqa_bin],
  'dither/t/0037_fixed_float32_fs_serpentine_lsqa.t': [lsqa_bin],
  'dither/t/0038_fixed_float32_atkinson_lsqa.t': [lsqa_bin],
  'dither/t/0039_fixed_float32_jajuni_lsqa.t': [lsqa_bin],
  'dither/t/0040_fixed_float32_stucki_lsqa.t': [lsqa_bin],
  'dither/t/0041_fixed_float32_burkes_lsqa.t': [lsqa_bin],
  'dither/t/0042_fixed_float32_sierra1_lsqa.t': [lsqa_bin],
  'dither/t/0043_fixed_float32_sierra2_lsqa.t': [lsqa_bin],
  'dither/t/0044_fixed_float32_sierra3_lsqa.t': [lsqa_bin],
  'cli-argument-shift/t/0005_cli_token_is_known_option.t':
    [test_runner_exe],
  'cli-argument-shift/t/0006_cli_option_requires_argument.t':
    [test_runner_exe],
  'cli-argument-shift/t/0007_cli_guard_missing_argument.t':
    [test_runner_exe],
}

core_depends = {
  'palette/t/0001_kmeans_init_default_unset.t':
    [test_runner_exe],
  'palette/t/0002_kmeans_init_auto.t':
    [test_runner_exe],
  'palette/t/0003_kmeans_init_unknown.t':
    [test_runner_exe],
  'palette/t/0004_kmeans_init_pca.t':
    [test_runner_exe],
  'palette/t/0005_kmeans_init_cache.t':
    [test_runner_exe],
  'palette/t/0006_palette_fallback_conversion.t':
    [test_runner_exe],
  'palette/t/0007_palette_fallback_output.t':
    [test_runner_exe],
  'probe/t/0001_probe_parse.t': [test_runner_exe],
}

# Match autotools TAP timeout to keep CI behavior consistent (seconds per file).
foreach t : cli_tests
  test(t, sh,
    args: [meson.current_source_dir() / t],
    protocol: 'tap',
    env: env,
    depends: cli_depends.get(t, []),
    timeout: 300,
    suite: ['tap'])
endforeach

foreach t : aborttrace_tests
  test(t, sh,
    args: [meson.current_source_dir() / t],
    protocol: 'tap',
    env: env,
    depends: cli_depends.get(t, []),
    is_parallel: false,
    timeout: 300,
    suite: ['tap'])
endforeach

foreach t : ['palette/t/0001_kmeans_init_default_unset.t',
             'palette/t/0002_kmeans_init_auto.t',
             'palette/t/0003_kmeans_init_unknown.t',
             'palette/t/0004_kmeans_init_pca.t',
             'palette/t/0005_kmeans_init_cache.t',
             'palette/t/0006_palette_fallback_conversion.t',
             'palette/t/0007_palette_fallback_output.t',
             'probe/t/0001_probe_parse.t']
  test(t, sh,
    args: [meson.current_source_dir() / t],
    protocol: 'tap',
    env: env,
    depends: core_depends.get(t, []),
    timeout: 120,
    suite: ['tap'])
endforeach

python_depends = [libsixel]

foreach t : ['0001_python_bindings.t', '0002_python_api_formats.t',
             '0003_python_api_options.t', '0004_python_api_errors.t',
             '0005_python_resource_management.t']
  test(t, sh,
    args: [meson.current_source_dir() / 'python' / 't' / t],
    protocol: 'tap',
    env: env,
    timeout: 300,
    depends: python_depends,
    suite: ['tap'])
endforeach

filter_tests = [
  'filter/t/0001_filter_clip.t',
  'filter/t/0002_filter_sample.t',
  'filter/t/0003_filter_resize.t',
  'filter/t/0004_filter_colors.t',
  'filter/t/0005_filter_lookup.t',
  'filter/t/0006_filter_final_merge.t',
  'filter/t/0007_filter_vpte.t',
  'filter/t/0008_filter_load.t',
  'filter/t/0009_filter_dither.t',
  'filter/t/0010_filter_encode.t',
]

foreach t : filter_tests
  test(t, sh,
    args: [meson.current_source_dir() / t],
    protocol: 'tap',
    env: env,
    depends: [test_runner_exe],
    timeout: 120,
    suite: ['tap'])
endforeach


# Register the loader tests unconditionally to mirror autotools.
# Each .t script is responsible for skipping when a dependency is missing.
loader_tests = [
  '0008_loader_coregraphics_pixelformat',
  '0009_loader_wic_pixelformat',
  '0010_loader_gdk_pixbuf_pixelformat',
  '0011_loader_gd_pixelformat',
  '0012_loader_libpng_pixelformat',
  '0013_loader_libjpeg_pixelformat',
  '0014_loader_builtin_pixelformat',
  '0015_loader_quicklook_pixelformat',
  '0016_loader_gnome_thumbnailer_pixelformat',
  '0017_loader_builtin_psd',
  '0018_loader_builtin_hdr',
  '0019_loader_builtin_pic',
]

foreach test_name : loader_tests
  test(test_name, sh,
    args: [meson.current_source_dir()
      / 'loader'
      / 't'
      / (test_name + '.t')],
    env: env,
    protocol: 'tap',
    timeout: 120,
    depends: [test_runner_exe],
    suite: ['tap'])
endforeach

if gdk_pixbuf_loader_deps.length() > 0
  gdk_pixbuf_loader_tests = [
    '0001_gdk_pixbuf_loader',
    '0002_incremental_load',
    '0003_corrupt_data',
    '0004_propagate_error',
    '0005_context_free',
    '0006_loader_module',
  ]
  if conf.has('HAVE_GDK_PIXBUF2') and get_option('img2sixel').enabled()
    gdk_pixbuf_loader_tests += ['0007_loader_plugin_integration']
  endif
  foreach test_name : gdk_pixbuf_loader_tests
    deps = [pixbufloader_sixel]
    if test_name == '0007_loader_plugin_integration'
      deps += [img2sixel_exe]
    endif
    test(test_name, sh,
      args: [meson.current_source_dir()
        / 'gdk-pixbuf-loader'
        / 't'
        / (test_name + '.t')],
      env: env,
      protocol: 'tap',
      suite: ['gdk-pixbuf-loader'],
      depends: deps)
  endforeach
endif

add_test_setup('ci', timeout_multiplier: 1)
