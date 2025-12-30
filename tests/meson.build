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
amalgamated_tools_enabled = get_option('amalgamated_tools').enabled()

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
             'cli-core/t/0002_invalid_option_arguments.t',
             'cli-core/t/0003_invalid_option_combinations.t',
             'cli-core/t/0004_stdin.t',
             'cli-core/t/0005_help.t',
             'cli-core/t/0005_version.t',
             'cli-core/t/0006_basic.t',
             'cli-core/t/0007_option_matching.t',
             'cli-core/t/0008_argument_shift.t',
             'cli-core/t/0009_cli_token_is_known_option.t',
             'cli-core/t/0010_cli_option_requires_argument.t',
             'cli-core/t/0011_cli_guard_missing_argument.t',
             'converter-options/t/0001_conversion_options_01.t',
             'converter-options/t/0002_conversion_options_02.t',
             'converter-options/t/0003_conversion_options_03.t',
             'converter-options/t/0004_conversion_options_04.t',
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
             'packaging/t/0001_python_wheel.t',
             'mapfile/t/0001_io.t',
            'pipeline/t/0001_pipeline_planner_baseline_run.t',
            'pipeline/t/0002_pipeline_planner_baseline_summary.t',
            'pipeline/t/0003_pipeline_planner_baseline_threads.t',
            'pipeline/t/0004_pipeline_planner_overrides_run.t',
            'pipeline/t/0005_pipeline_planner_overrides_summary.t',
            'pipeline/t/0006_pipeline_planner_overrides_threads.t',
            'resize/t/0001_resize_precision_mode_run.t',
            'resize/t/0002_resize_precision_mode_scaler_input.t',
            'resize/t/0003_resize_precision_mode_post_colorspace.t',
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
  }
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
    test(test_name, target,
      env: env,
      suite: ['gdk-pixbuf-loader'])
  endforeach
endif

add_test_setup('ci', timeout_multiplier: 1)
