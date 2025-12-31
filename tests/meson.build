sh = find_program('sh')
artifact_root = meson.current_build_dir() / '_artifacts'
env = environment()
env.set('ARTIFACT_ROOT', artifact_root)
env.set('TOP_SRCDIR', meson.project_source_root())
env.set('TOP_BUILDDIR', meson.project_build_root())
env.set('MESON_SOURCE_ROOT', meson.project_source_root())
env.set('MESON_BUILD_ROOT', meson.project_build_root())
# Ensure Python TAP tests can locate the freshly built shared library when
# Meson is invoked without a prior `meson compile`. The binding smoke test
# relies on LIBSIXEL_LIBDIR to point at the build artifact directory.
env.set('LIBSIXEL_LIBDIR', meson.project_build_root() / 'src')

cli_helper_sources = {
  '0029_cli_token_is_known_option':
    ['cli/0029_cli_token_is_known_option.c',
     '../converters/cli.c'],
  '0030_cli_option_requires_argument':
    ['cli/0030_cli_option_requires_argument.c',
     '../converters/cli.c'],
  '0031_cli_guard_missing_argument':
    ['cli/0031_cli_guard_missing_argument.c',
     '../converters/cli.c'],
}

cli_helper_targets = {}
foreach helper_name, helper_sources : cli_helper_sources
  cli_helper_targets += {helper_name: executable(helper_name, helper_sources,
    include_directories: [project_inc, include_directories('..')],
    dependencies: [libsixel_dep],
    install: false)}
endforeach

core_helper_sources = {
  '0001_kmeans_init': ['palette/0001_kmeans_init.c'],
  '0001_probe_parse': ['probe/0001_probe_parse.c'],
}

core_helper_targets = {}
foreach helper_name, helper_sources : core_helper_sources
  core_helper_targets += {helper_name: executable(helper_name, helper_sources,
    include_directories: [project_inc, include_directories('..')],
    dependencies: [libsixel_dep],
    install: false)}
endforeach

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

cli_tests = ['cli-core/t/0001_invalid_positional.t',
             'cli-core/t/0002_invalid_option_arguments_unknown_dither.t',
             'cli-core/t/0002_invalid_option_arguments_unknown_resize_filter.t',
             'cli-core/t/0002_invalid_option_arguments_unknown_scaling_mode.t',
             'cli-core/t/0002_invalid_option_arguments_unknown_tone_adjustment.t',
             'cli-core/t/0002_invalid_option_arguments_invalid_width.t',
             'cli-core/t/0002_invalid_option_arguments_invalid_height.t',
             'cli-core/t/0002_invalid_option_arguments_invalid_format.t',
             'cli-core/t/0002_invalid_option_arguments_invalid_quality.t',
             'cli-core/t/0002_invalid_option_arguments_invalid_layout.t',
             'cli-core/t/0002_invalid_option_arguments_invalid_bpp.t',
             'cli-core/t/0002_invalid_option_arguments_invalid_encoder_tweak.t',
             'cli-core/t/0002_invalid_option_arguments_invalid_background_colour.t',
             'cli-core/t/0002_invalid_option_arguments_missing_background_component.t',
             'cli-core/t/0002_invalid_option_arguments_overlong_background.t',
             'cli-core/t/0002_invalid_option_arguments_malformed_hex_background.t',
             'cli-core/t/0002_invalid_option_arguments_unknown_named_colour.t',
             'cli-core/t/0002_invalid_option_arguments_incomplete_rgb_background.t',
             'cli-core/t/0002_invalid_option_arguments_unsupported_legacy_width.t',
             'cli-core/t/0002_invalid_option_arguments_missing_palette_file.t',
             'cli-core/t/0002_invalid_option_arguments_invalid_colour_space_index.t',
             'cli-core/t/0003_invalid_option_combinations_palette_and_encode.t',
             'cli-core/t/0003_invalid_option_combinations_inspect_with_palette.t',
             'cli-core/t/0003_invalid_option_combinations_palette_with_terminal_preset.t',
             'cli-core/t/0003_invalid_option_combinations_eight_bit_with_palette_dump.t',
             'cli-core/t/0004_stdin.t',
             'cli-core/t/0005_help.t',
             'cli-core/t/0005_version.t',
             'cli-core/t/0006_basic_missing_input.t',
             'cli-core/t/0006_basic_help_option.t',
             'cli-core/t/0006_basic_version_option.t',
             'cli-core/t/0006_basic_stdin_snake.t',
             'cli-core/t/0006_basic_stdin_map8.t',
             'cli-core/t/0006_basic_stdin_stdout_map64.t',
             'cli-core/t/0006_basic_file_arguments_snake.t',
             'cli-core/t/0006_basic_direct_output.t',
             'cli-core/t/0006_basic_direct_with_dequantize_conflict.t',
             'cli-core/t/0006_basic_png_prefix_path.t',
             'cli-core/t/0006_basic_png_stdout.t',
             'cli-core/t/0006_basic_png_prefix_missing_target.t',
             'cli-core/t/0006_basic_dequantize_unique_prefix.t',
            'cli-core/t/0006_basic_parallel_indexed.t',
            'cli-core/t/0006_basic_parallel_direct.t',
            'cli-core/t/0006_basic_parallel_cli_override.t',
            'cli-core/t/0006_basic_invalid_threads_token.t',
            'cli-core/t/0007_option_matching_prefix_unique.t',
            'cli-core/t/0007_option_matching_prefix_ambiguous.t',
            'cli-core/t/0007_option_matching_diffusion_correction.t',
            'cli-core/t/0007_option_matching_distance1_multi.t',
            'cli-core/t/0007_option_matching_distance2.t',
            'cli-core/t/0007_option_matching_distance3.t',
            'cli-core/t/0008_argument_shift_missing_mapfile.t',
            'cli-core/t/0008_argument_shift_outfile_named_like_option.t',
            'cli-core/t/0008_argument_shift_assessment_named_like_option.t',
            'cli-core/t/0009_cli_token_is_known_option.t',
            'cli-core/t/0010_cli_option_requires_argument.t',
            'cli-core/t/0011_cli_guard_missing_argument.t',
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
             'dither/t/0001_roundtrip_dither_gamma.t',
             'dither/t/0002_inspection_with_diffusion.t',
             'dither/t/0003_ascii_ppm_inspection_diffusion.t',
             'dither/t/0004_ascii_pbm_atkinson_inspection.t',
             'dither/t/0005_cropping_with_x_dither.t',
             'dither/t/0006_cropping_with_alternate_dither.t',
             'dither/t/0007_sixel_inspection_alternate_dither.t',
             'dither/t/0008_sixel_inspection_x_dither.t',
             'dither/t/0009_lanczos2_without_diffusion.t',
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
             'decoder/t/0001_dcs_arguments.t',
             'decoder/t/0002_tab_separated_colour_introducers.t',
             'decoder/t/0003_oversized_dcs_geometry.t',
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
             'regression/t/0005_lsqa_regression.t',
             'docs/t/0001_help_vs_man.t',
             'docs/t/0002_man_vs_bash_completion.t',
             'docs/t/0003_bash_vs_zsh_completion.t',
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

cli_depends = {
  'regression/t/0005_lsqa_regression.t': [lsqa_bin],
  'cli-core/t/0009_cli_token_is_known_option.t':
    [cli_helper_targets['0029_cli_token_is_known_option']],
  'cli-core/t/0010_cli_option_requires_argument.t':
    [cli_helper_targets['0030_cli_option_requires_argument']],
  'cli-core/t/0011_cli_guard_missing_argument.t':
    [cli_helper_targets['0031_cli_guard_missing_argument']],
}

core_depends = {
  'palette/t/0001_kmeans_init_default_unset.t':
    [core_helper_targets['0001_kmeans_init']],
  'palette/t/0002_kmeans_init_auto.t':
    [core_helper_targets['0001_kmeans_init']],
  'palette/t/0003_kmeans_init_unknown.t':
    [core_helper_targets['0001_kmeans_init']],
  'palette/t/0004_kmeans_init_pca.t':
    [core_helper_targets['0001_kmeans_init']],
  'palette/t/0005_kmeans_init_cache.t':
    [core_helper_targets['0001_kmeans_init']],
  'palette/t/0006_palette_fallback_conversion.t':
    [core_helper_targets['0001_kmeans_init']],
  'palette/t/0007_palette_fallback_output.t':
    [core_helper_targets['0001_kmeans_init']],
  'probe/t/0001_probe_parse.t': [core_helper_targets['0001_probe_parse']],
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

filter_sources = {
  '0001_filter_clip': 'filter/0001_filter_clip.c',
  '0002_filter_sample': 'filter/0002_filter_sample.c',
  '0003_filter_resize': 'filter/0003_filter_resize.c',
  '0004_filter_colors': 'filter/0004_filter_colors.c',
  '0005_filter_lookup': 'filter/0005_filter_lookup.c',
  '0006_filter_final_merge': 'filter/0006_filter_final_merge.c',
  '0007_filter_vpte': 'filter/0007_filter_vpte.c',
  '0008_filter_load': 'filter/0008_filter_load.c',
  '0009_filter_dither': 'filter/0009_filter_dither.c',
  '0010_filter_encode': 'filter/0010_filter_encode.c',
}

filter_test_inc = [project_inc, include_directories('filter')]
filter_units = {}
foreach unit_name, unit_source : filter_sources
  if amalgamated_tools_enabled
    define_flag = '-DBUILD_' + unit_name.to_upper() + '=1'
    # Propagate the build guard to both C and ObjC translations so macOS
    # builds enable the test harness embedded in the amalgamated source.
    unit_exe = executable(unit_name, amalgamation_out,
      c_args: libsixel_posix_defs + [define_flag],
      objc_args: libsixel_posix_defs + [define_flag],
      include_directories: filter_test_inc,
      dependencies: deps,
      install: false)
  else
    unit_exe = executable(unit_name, [unit_source],
      dependencies: [libsixel_dep],
      include_directories: filter_test_inc,
      install: false)
  endif
  if shebang_enabled
    shebang_targets += [custom_target(unit_name + '_shebang',
      input: unit_exe,
      output: unit_name + '.shebang.stamp',
      command: [shebang_helper, shebang_file, '@INPUT@'],
      depend_files: shebang_file,
      build_by_default: true)]
  endif
  filter_units += { unit_name: unit_exe }
endforeach

test('0001_filter_clip', filter_units['0001_filter_clip'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('0002_filter_sample', filter_units['0002_filter_sample'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('0003_filter_resize', filter_units['0003_filter_resize'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('0004_filter_colors', filter_units['0004_filter_colors'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('0005_filter_lookup', filter_units['0005_filter_lookup'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('0006_filter_final_merge', filter_units['0006_filter_final_merge'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('0007_filter_vpte', filter_units['0007_filter_vpte'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('0008_filter_load', filter_units['0008_filter_load'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('0009_filter_dither', filter_units['0009_filter_dither'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('0010_filter_encode', filter_units['0010_filter_encode'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

if gdk_pixbuf_loader_deps.length() > 0
  gdk_pixbuf_loader_sources = [
    '../gdk-pixbuf-loader/sixel-pixbuf-loader.c',
  ]
  gdk_pixbuf_loader_tests = {
    '0001_gdk_pixbuf_loader': 'gdk-pixbuf-loader/0001_gdk_pixbuf_loader.c',
    '0002_incremental_load':
      'gdk-pixbuf-loader/0002_incremental_load.c',
    '0003_corrupt_data': 'gdk-pixbuf-loader/0003_corrupt_data.c',
    '0004_propagate_error':
      'gdk-pixbuf-loader/0004_propagate_error.c',
    '0005_context_free': 'gdk-pixbuf-loader/0005_context_free.c',
    '0006_loader_module': 'gdk-pixbuf-loader/0006_loader_module.c',
  }
  if conf.has('HAVE_GDK_PIXBUF2') and get_option('img2sixel').enabled()
    gdk_pixbuf_loader_tests += {
      '0007_loader_plugin_integration':
        'gdk-pixbuf-loader/0007_loader_plugin_integration.c',
    }
  endif
  foreach test_name, source_file : gdk_pixbuf_loader_tests
    target = executable(test_name,
      [source_file] + gdk_pixbuf_loader_sources,
      include_directories: project_inc,
      dependencies: gdk_pixbuf_loader_deps + [libsixel_dep],
      c_args: ['-DGDK_PIXBUF_ENABLE_BACKEND',
               '-DSIXEL_PIXBUF_LOADER_TESTING'],
      install: false)
    if shebang_enabled
      shebang_targets += [custom_target(test_name + '-shebang',
        input: target,
        output: test_name + '.shebang.stamp',
        command: [shebang_helper, shebang_file, '@INPUT@'],
        depend_files: shebang_file,
        build_by_default: true)]
    endif
    deps = [pixbufloader_sixel]
    if test_name == '0007_loader_plugin_integration'
      deps += [img2sixel_exe]
    endif
    test(test_name, target,
      env: env,
      suite: ['gdk-pixbuf-loader'],
      depends: deps)
  endforeach
endif

add_test_setup('ci', timeout_multiplier: 1)
