sh = find_program('sh')
artifact_root = meson.current_build_dir() / '_artifacts'
cross_compiling = 'no'
if meson.is_cross_build()
  cross_compiling = 'yes'
endif

sanitize_opt = get_option('b_sanitize')
sixel_tsan_build = 'no'
sixel_msan_build = 'no'
if sanitize_opt.contains('thread')
  sixel_tsan_build = 'yes'
endif
if sanitize_opt.contains('memory')
  sixel_msan_build = 'yes'
endif

bin_ext = ''
if host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
  bin_ext = '.exe'
endif

base_env_settings = [
  ['ARTIFACT_ROOT', artifact_root],
  ['TOP_SRCDIR', meson.project_source_root()],
  ['TOP_BUILDDIR', meson.project_build_root()],
  ['MESON_SOURCE_ROOT', meson.project_source_root()],
  ['MESON_BUILD_ROOT', meson.project_build_root()],
  ['CROSS_COMPILING', cross_compiling],
  ['SIXEL_TSAN_BUILD', sixel_tsan_build],
  ['SIXEL_MSAN_BUILD', sixel_msan_build],
  ['ENABLE_PYTHON', (get_option('python').disabled() ? '0' : '1')],
  # Mirror autotools check-TESTS metadata so TAP helper scripts can share
  # one execution path across build systems.
  # Ensure Python TAP tests can locate the freshly built shared library when
  # Meson is invoked without a prior `meson compile`. The binding smoke test
  # relies on LIBSIXEL_LIBDIR to point at the build artifact directory.
  ['LIBSIXEL_LIBDIR', meson.project_build_root() / 'src'],
  ['IMG2SIXEL_PATH', meson.project_build_root() / 'converters' /
   ('img2sixel' + bin_ext)],
  ['SIXEL2PNG_PATH', meson.project_build_root() / 'converters' /
   ('sixel2png' + bin_ext)],
  ['LSQA_PATH', meson.project_build_root() / 'assessment' /
   ('lsqa' + bin_ext)],
  ['TEST_RUNNER_PATH', meson.project_build_root() / 'tests' /
   ('test_runner' + bin_ext)],
]

# Meson sets runtime library lookup via rpath/runpath, so test wrappers must
# not override PATH/LD_LIBRARY_PATH/DYLD_LIBRARY_PATH in this execution path.
base_env_settings += [['SIXEL_SHLIBPATH_OVERRIDES_RUNPATH', 'no']]

# Keep feature toggle exports in sync with autotools check-TESTS.
config_macro_env_settings = []
config_macro_env_text = run_command(
  python3,
  '-c',
  '''import pathlib
import re
import sys

config_h = pathlib.Path(sys.argv[1])
if not config_h.exists():
    raise SystemExit(0)

for line in config_h.read_text(encoding="utf-8", errors="ignore").splitlines():
    m_define = re.match(r"^#define ([A-Z0-9_]+) 1$", line)
    if m_define:
        print(f"{m_define.group(1)}=1")
        continue
    m_undef = re.match(r"^/\\* #undef ([A-Z0-9_]+) \\*/$", line)
    if m_undef:
        print(f"{m_undef.group(1)}=0")
''',
  meson.project_build_root() / 'config.h',
  check: true,
).stdout().strip()
if config_macro_env_text != ''
  foreach macro_line : config_macro_env_text.split('\n')
    macro_pair = macro_line.split('=')
    if macro_pair.length() == 2
      config_macro_env_settings += [[macro_pair[0], macro_pair[1]]]
    endif
  endforeach
endif
base_env_settings += config_macro_env_settings

if host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
  # Most TAP shell wrappers reference built tools via ${SIXEL_BIN_EXT}.
  # Meson does not export this automatically, so Windows jobs otherwise look
  # up extension-less paths and mark many tests as skipped.
  base_env_settings += [['SIXEL_BIN_EXT', '.exe']]
endif

env = environment()
foreach setting : base_env_settings
  env.set(setting[0], setting[1])
endforeach

test_runner_sources = [
  'test_runner.c',
  'cli/aborttrace/0001_img2sixel_aborttrace.c',
  'cli/0029_cli_token_is_known_option.c',
  'cli/0030_cli_option_requires_argument.c',
  'cli/0031_cli_guard_missing_argument.c',
  'processing/filter/0001_filter_clip.c',
  'processing/filter/0002_filter_sample.c',
  'processing/filter/0003_filter_resize.c',
  'processing/filter/0004_filter_colors.c',
  'processing/filter/0005_filter_lookup.c',
  'processing/filter/0006_filter_final_merge.c',
  'processing/filter/0007_filter_vpte.c',
  'processing/filter/0008_filter_load.c',
  'processing/filter/0009_filter_dither.c',
  'processing/filter/0010_filter_encode.c',
  'processing/filter/filter_test_common.h',
  'io/loader/0008_loader_coregraphics_pixelformat.c',
  'io/loader/0009_loader_wic_pixelformat.c',
  'io/loader/0010_loader_gdk_pixbuf_pixelformat.c',
  'io/loader/0011_loader_gd_pixelformat.c',
  'io/loader/0012_loader_libpng_pixelformat.c',
  'io/loader/0013_loader_libjpeg_pixelformat.c',
  'io/loader/0014_loader_builtin_pixelformat.c',
  'io/loader/0015_loader_quicklook_pixelformat.c',
  'io/loader/0016_loader_gnome_thumbnailer_pixelformat.c',
  'io/loader/0017_loader_libwebp_pixelformat.c',
  'io/loader/pixelformat_test_common.h',
  'quant/palette/0001_kmeans_init.c',
  'diagnostics/probe/0001_probe_parse.c',
  'extension/gdk-pixbuf-loader/0001_gdk_pixbuf_loader.c',
  'extension/gdk-pixbuf-loader/0002_incremental_load.c',
  'extension/gdk-pixbuf-loader/0003_corrupt_data.c',
  'extension/gdk-pixbuf-loader/0004_propagate_error.c',
  'extension/gdk-pixbuf-loader/0005_context_free.c',
  'extension/gdk-pixbuf-loader/0006_loader_module.c',
  'extension/gdk-pixbuf-loader/0007_loader_plugin_integration.c',
  '../gdk-pixbuf-loader/sixel-pixbuf-loader.c',
  '../converters/aborttrace.c',
  '../converters/cli.c',
]

test_runner_inc = [
  project_inc,
  include_directories('..'),
]

test_runner_exe = executable('test_runner', test_runner_sources,
  include_directories: test_runner_inc,
  dependencies: [libsixel_dep, gdk_pixbuf_loader_deps, abort_trace_deps],
  c_args: ['-DGDK_PIXBUF_ENABLE_BACKEND',
           '-DSIXEL_PIXBUF_LOADER_TESTING',
           '-DSIXEL_ENABLE_GDK_PIXBUF_LOADER_TESTS'],
  install: false)

test_deps = [test_runner_exe, lsqa_bin] + img2sixel_deps + sixel2png_deps
if not get_option('python').disabled()
  test_deps += [python_wheel_target]
endif

is_windows = host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
if is_windows
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

tests_text = run_command(
  sh,
  meson.project_source_root() / 'build-aux/read-check-test-list.sh',
  'tests',
  meson.current_source_dir() / 'check-tests.list',
  check: true,
).stdout().strip()
tests = []
if tests_text != ''
  tests = tests_text.split()
endif

# Match autotools TAP timeout to keep CI behavior consistent (seconds per file).
# Ensure Meson builds the TAP helper binary before running shell-based tests.
foreach t : tests
  parts = t.split('/')
  test_base = parts[parts.length() - 1]
  test_category = ''
  if parts.length() > 1
    foreach idx : range(0, parts.length() - 1)
      if test_category == ''
        test_category = parts[idx]
      else
        test_category = test_category + '/' + parts[idx]
      endif
    endforeach
  endif
  if test_category == ''
    test_category = '.'
  endif
  test_dir = meson.current_source_dir()
  if test_category != '.'
    test_dir = test_dir / test_category
  endif
  test_artifact_dir = artifact_root / test_category / test_base
  test_env = environment()
  foreach setting : base_env_settings
    test_env.set(setting[0], setting[1])
  endforeach
  if is_windows
    foreach dll_path : dll_paths
      test_env.prepend('PATH', dll_path)
    endforeach
  endif
  test_env.set('test_dir', test_dir)
  test_env.set('category_name', test_category)
  test_env.set('ARTIFACT_LOCAL_DIR', test_artifact_dir)
  test_env.set('SIXEL_TEST_PYTHON_VENV', meson.project_build_root() /
    'tests/.python-test-venv')
  test(t, sh,
    args: ['-c', '''
mkdir -p "$ARTIFACT_LOCAL_DIR"

if [ "x${ENABLE_PYTHON:-0}" = "x1" ]; then
  shared_python_venv="${SIXEL_TEST_PYTHON_VENV}"
  python_bin=$(command -v python3 || command -v python || true)
  wheel_path=$(find "${MESON_BUILD_ROOT}/python/dist" -maxdepth 1 \
    -type f -name 'libsixel_wheel-*.whl' 2>/dev/null | head -n 1)
  if [ -n "$python_bin" ] && [ -n "$wheel_path" ]; then
    if "$python_bin" -c 'import importlib.util,sys; missing=[m for m in ("venv","ensurepip") if importlib.util.find_spec(m) is None]; sys.exit(0 if not missing else 1)' >/dev/null 2>&1; then
      if [ ! -x "$shared_python_venv/bin/python" ]; then
        rm -rf "$shared_python_venv"
        "$python_bin" -m venv "$shared_python_venv" >/dev/null 2>&1 || true
      fi
      if [ -x "$shared_python_venv/bin/python" ]; then
        if ! "$shared_python_venv/bin/python" -c 'import importlib.util,sys; sys.exit(0 if importlib.util.find_spec("libsixel_wheel") else 1)' >/dev/null 2>&1; then
          "$shared_python_venv/bin/python" -m pip install --no-deps "$wheel_path" >/dev/null 2>&1 || true
        fi
        if ! "$shared_python_venv/bin/python" -c 'import importlib.util,sys; sys.exit(0 if importlib.util.find_spec("libsixel_wheel") else 1)' >/dev/null 2>&1; then
          export SIXEL_TEST_PYTHON_VENV=
        fi
      fi
    fi
  fi
fi

exec "$@"
''', 'sh', meson.current_source_dir() / t],
    depends: test_deps,
    protocol: 'tap',
    env: test_env,
    timeout: 90,
    suite: ['tap'])
endforeach

add_test_setup('ci', timeout_multiplier: 1)
