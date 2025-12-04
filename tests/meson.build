sh = find_program('sh')
artifact_root = meson.current_build_dir() / '_artifacts'
env = environment()
env.set('ARTIFACT_ROOT', artifact_root)
env.set('TOP_SRCDIR', meson.project_source_root())
env.set('TOP_BUILDDIR', meson.project_build_root())

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

foreach t : ['test_0001_01_invalid_positional.t',
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
             'test_0018_lsqa_regression.t']
  test(t, sh,
    args: [meson.current_source_dir() / 't' / t],
    protocol: 'tap',
    env: env,
    timeout: 300,
    suite: ['tap'])
endforeach

add_test_setup('ci', timeout_multiplier: 1)
