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

cli_tests = ['test_0001_01_invalid_positional.t',
             'test_0001_02_invalid_option_arguments.t',
             'test_0001_03_invalid_option_combinations.t',
             'test_0002_stdin.t', 'test_0003_information.t',
             'test_0004_conversion_options_01.t',
             'test_0004_conversion_options_02.t',
             'test_0004_conversion_options_03.t',
             'test_0004_conversion_options_04.t',
             'test_0005_dcs_arguments.t',
             'test_0006_dcs_variations.t',
             'test_0007_animation.t',
             'test_0008_progressive_jpeg.t',
             'test_0009_pngsuite.t',
             'test_0010_curl.t',
             'test_0011_basic.t',
             'test_0012_regressions.t',
             'test_0013_documentation.t',
             'test_0014_option_matching.t',
             'test_0015_argument_shift.t',
             'test_0016_assessment_spool.t',
             'test_0017_clipboard.t',
             'test_0018_lsqa_regression.t',
             'test_0019_python_wheel.t',
             'test_0020_mapfile_io.t',
             'test_0021_palette_fallback.t',
             'test_0021_pipeline_planner.t',
             'test_0022_resize_precision_mode.t']

cli_depends = {
  'test_0018_lsqa_regression.t': [lsqa_bin],
}

# Match autotools TAP timeout to keep CI behavior consistent (seconds per file).
foreach t : cli_tests
  test(t, sh,
    args: [meson.current_source_dir() / 'cli' / 't' / t],
    protocol: 'tap',
    env: env,
    depends: cli_depends.get(t, []),
    timeout: 300,
    suite: ['tap'])
endforeach

python_depends = [libsixel]

foreach t : ['test_0001_python_bindings.t', 'test_0002_python_api_formats.t',
             'test_0003_python_api_options.t', 'test_0004_python_api_errors.t',
             'test_0005_python_resource_management.t']
  test(t, sh,
    args: [meson.current_source_dir() / 'python' / 't' / t],
    protocol: 'tap',
    env: env,
    timeout: 300,
    depends: python_depends,
    suite: ['tap'])
endforeach

filter_sources = {
  'filter_tests': 'filter/filter_tests.c',
  'filter_sample_tests': 'filter/filter_sample_tests.c',
  'filter_resize_tests': 'filter/filter_resize_tests.c',
  'filter_colors_tests': 'filter/filter_colors_tests.c',
  'filter_lookup_tests': 'filter/filter_lookup_tests.c',
  'filter_final_merge_tests': 'filter/filter_final_merge_tests.c',
  'filter_vpte_tests': 'filter/filter_vpte_tests.c',
  'filter_load_tests': 'filter/filter_load_tests.c',
  'filter_dither_tests': 'filter/filter_dither_tests.c',
  'filter_encode_tests': 'filter/filter_encode_tests.c',
}

filter_inc = include_directories('filter')

filter_units = {}
foreach unit_name, unit_source : filter_sources
  if amalgamated_tools_enabled
    define_flag = '-DBUILD_' + unit_name.to_upper() + '=1'
    unit_exe = executable(unit_name, amalgamation_out,
      c_args: libsixel_posix_defs + [define_flag],
      include_directories: [project_inc, filter_inc],
      dependencies: deps,
      install: false)
  else
    unit_exe = executable(unit_name, [unit_source],
      dependencies: [libsixel_dep],
      include_directories: [project_inc, filter_inc],
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

test('test_3000_filter_clip', filter_units['filter_tests'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3001_filter_sample', filter_units['filter_sample_tests'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3002_filter_resize', filter_units['filter_resize_tests'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3003_filter_colors', filter_units['filter_colors_tests'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3004_filter_lookup', filter_units['filter_lookup_tests'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3005_filter_final_merge', filter_units['filter_final_merge_tests'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3006_filter_vpte', filter_units['filter_vpte_tests'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3007_filter_load', filter_units['filter_load_tests'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3008_filter_dither', filter_units['filter_dither_tests'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3009_filter_encode', filter_units['filter_encode_tests'],
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

if gdk_pixbuf_loader_deps.length() > 0
  gdk_pixbuf_loader_sources = [
    'gdk-pixbuf-loader/test-gdk-pixbuf-loader.c',
    '../gdk-pixbuf-loader/sixel-pixbuf-loader.c',
  ]
  gdk_pixbuf_loader_test = executable('gdk-pixbuf-loader-test',
    gdk_pixbuf_loader_sources,
    include_directories: project_inc,
    dependencies: gdk_pixbuf_loader_deps + [libsixel_dep],
    c_args: ['-DGDK_PIXBUF_ENABLE_BACKEND'],
    install: false)
  if shebang_enabled
    shebang_targets += [custom_target('gdk-pixbuf-loader-test-shebang',
      input: gdk_pixbuf_loader_test,
      output: 'gdk-pixbuf-loader-test.shebang.stamp',
      command: [shebang_helper, shebang_file, '@INPUT@'],
      depend_files: shebang_file,
      build_by_default: true)]
  endif
  test('gdk-pixbuf-loader', gdk_pixbuf_loader_test,
    env: env,
    suite: ['gdk-pixbuf-loader'])
endif

add_test_setup('ci', timeout_multiplier: 1)
