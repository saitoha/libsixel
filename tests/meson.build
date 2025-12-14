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
             'test_0021_palette_fallback.t']

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

filter_unit = executable('filter_tests',
  ['filter/filter_tests.c'],
  dependencies: [libsixel_dep],
  include_directories: project_inc,
  install: false)

filter_sample_unit = executable('filter_sample_tests',
  ['filter/filter_sample_tests.c'],
  dependencies: [libsixel_dep],
  include_directories: project_inc,
  install: false)

filter_resize_unit = executable('filter_resize_tests',
  ['filter/filter_resize_tests.c'],
  dependencies: [libsixel_dep],
  include_directories: project_inc,
  install: false)

filter_colors_unit = executable('filter_colors_tests',
  ['filter/filter_colors_tests.c'],
  dependencies: [libsixel_dep],
  include_directories: project_inc,
  install: false)

filter_lookup_unit = executable('filter_lookup_tests',
  ['filter/filter_lookup_tests.c'],
  dependencies: [libsixel_dep],
  include_directories: project_inc,
  install: false)

filter_final_merge_unit = executable('filter_final_merge_tests',
  ['filter/filter_final_merge_tests.c'],
  dependencies: [libsixel_dep],
  include_directories: project_inc,
  install: false)

filter_vpte_unit = executable('filter_vpte_tests',
  ['filter/filter_vpte_tests.c'],
  dependencies: [libsixel_dep],
  include_directories: project_inc,
  install: false)

filter_load_unit = executable('filter_load_tests',
  ['filter/filter_load_tests.c'],
  dependencies: [libsixel_dep],
  include_directories: project_inc,
  install: false)

filter_dither_unit = executable('filter_dither_tests',
  ['filter/filter_dither_tests.c'],
  dependencies: [libsixel_dep],
  include_directories: project_inc,
  install: false)

filter_encode_unit = executable('filter_encode_tests',
  ['filter/filter_encode_tests.c'],
  dependencies: [libsixel_dep],
  include_directories: project_inc,
  install: false)

test('test_3000_filter_clip', filter_unit,
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3001_filter_sample', filter_sample_unit,
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3002_filter_resize', filter_resize_unit,
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3003_filter_colors', filter_colors_unit,
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3004_filter_lookup', filter_lookup_unit,
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3005_filter_final_merge', filter_final_merge_unit,
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3006_filter_vpte', filter_vpte_unit,
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3007_filter_load', filter_load_unit,
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3008_filter_dither', filter_dither_unit,
  env: env,
  protocol: 'tap',
  timeout: 120,
  suite: ['tap'])

test('test_3009_filter_encode', filter_encode_unit,
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
  test('gdk-pixbuf-loader', gdk_pixbuf_loader_test,
    env: env,
    suite: ['gdk-pixbuf-loader'])
endif

add_test_setup('ci', timeout_multiplier: 1)
