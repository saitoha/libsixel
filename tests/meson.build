sh = find_program('sh')
cp_cmd = find_program('cp', required: true)
actionlint_prog = find_program('actionlint', required: false)
shellcheck_prog = find_program('shellcheck', required: false)
codespell_prog = find_program('codespell', required: false)
python_for_tests = find_program('python3', required: false)
if not get_option('python').disabled() and not python_for_tests.found()
  error('python option is enabled but python3 executable was not found')
endif

ruby_for_tests_path = ''
if have_ruby
  ruby_for_tests_path = ruby.full_path()
endif

perl_for_tests_path = ''
if have_perl
  perl_for_tests_path = perl.full_path()
endif
perl_tests_enabled = have_perl
php_for_tests_path = ''
if have_php
  php_for_tests_path = php.full_path()
endif

# Keep Meson test-time Python resolution aligned with the interpreter selected
# in the project root. Using a different binary here can make wheel builds pass
# while Python TAP tests are skipped because the fallback interpreter lacks
# venv/ensurepip support.
python_for_tests_path = ''
if py_opt.enabled() and py.found()
  python_for_tests_path = py.full_path()
elif python_for_tests.found()
  python_for_tests_path = python_for_tests.full_path()
endif

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
  ['SIXEL_CONFIGURED_PYTHON', python_for_tests_path],
  ['ENABLE_RUBY', (have_ruby ? '1' : '0')],
  ['SIXEL_CONFIGURED_RUBY', ruby_for_tests_path],
  ['ENABLE_PERL', (perl_tests_enabled ? '1' : '0')],
  ['SIXEL_CONFIGURED_PERL', perl_for_tests_path],
  ['ENABLE_PHP', (have_php ? '1' : '0')],
  ['SIXEL_CONFIGURED_PHP', php_for_tests_path],
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

thumbnailer_case_dir = join_paths(
  meson.current_build_dir(),
  'data/inputs/thumbnailer/cases/0040/thumbnailers',
)
thumbnailer_case_src = join_paths(
  meson.current_source_dir(),
  'data/inputs/thumbnailer/cases/0040/thumbnailers',
)
run_command(
  sh,
  '-c',
  'set -eu; out_dir="$1"; cp_cmd="$2"; src_dir="$3"; ' +
  'mkdir -p "$out_dir"; ' +
  'sed "s|@CP_CMD@|${cp_cmd}|g" ' +
  '"$src_dir/valid-absolute.thumbnailer.in" ' +
  '>"$out_dir/valid-absolute.thumbnailer"; ' +
  'cat "$src_dir/missing-absolute.thumbnailer.in" ' +
  '>"$out_dir/missing-absolute.thumbnailer"',
  'meson-thumbnailer-gen',
  thumbnailer_case_dir,
  cp_cmd.full_path(),
  thumbnailer_case_src,
  check: true,
)

# Meson sets runtime library lookup via rpath/runpath, so test wrappers must
# not override PATH/LD_LIBRARY_PATH/DYLD_LIBRARY_PATH in this execution path.
base_env_settings += [['SIXEL_SHLIBPATH_OVERRIDES_RUNPATH', 'no']]

# Keep feature toggle exports in sync with autotools check-TESTS.
config_macro_env_settings = []
config_macro_pairs = run_command(
  sh,
  meson.project_source_root() / 'build-aux/resolve-config-h-exports.sh',
  meson.project_build_root() / 'config.h',
  'pairs',
  check: true,
).stdout().strip()
if config_macro_pairs != ''
  foreach macro_line : config_macro_pairs.split('\n')
    macro_pair = macro_line.split('\t')
    if macro_pair.length() == 2
      config_macro_env_settings += [[macro_pair[0], macro_pair[1]]]
    endif
  endforeach
endif
base_env_settings += config_macro_env_settings

base_env_settings += [['SIXEL_TEST_MACOS_PRODUCT_VERSION',
                       run_command(
                         sh,
                         '-c',
                         'sw_vers -productVersion 2>/dev/null || printf ""',
                         check: true,
                       ).stdout().strip()]]
base_env_settings += [['SIXEL_TEST_HOST_ARCH', host_machine.cpu_family()]]

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

# Collect test runner sources under tests/ dynamically so additions/removals
# do not require manual Meson updates.
test_runner_tests_sources_text = run_command(
  sh,
  '-c',
  'set -eu; cd "$1"; ' +
  'find . ' +
  '\\( -path "./.perl-test-venv" -o -path "./.python-test-venv" ' +
  '-o -path "./.ruby-test-venv" -o -path "./.php-test-venv" ' +
  '-o -path "./_artifacts" \\) ' +
  '-prune -o -type f \\( -name "*.c" -o -name "*.h" \\) -print | ' +
  'LC_ALL=C sort | sed "s|^\\./||"',
  'meson-test-runner-sources',
  meson.current_source_dir(),
  check: true,
).stdout().strip()
test_runner_tests_sources = []
if test_runner_tests_sources_text != ''
  test_runner_tests_sources = files(test_runner_tests_sources_text.split('\n'))
endif

test_runner_sources = test_runner_tests_sources + files(
  '../gdk-pixbuf-loader/sixel-pixbuf-loader.c',
  '../converters/aborttrace.c',
  '../converters/cli.c',
)

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
if not get_option('ruby').disabled()
  test_deps += [ruby_gem_target]
endif
if not get_option('php').disabled()
  test_deps += [php_package_target]
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
  meson.current_source_dir(),
  check: true,
).stdout().strip()
tests = []
if tests_text != ''
  tests = tests_text.split()
endif

# Match autotools TAP timeout to keep CI behavior consistent (seconds per file).
# Ensure Meson builds the TAP helper binary before running shell-based tests.

python_tap_wrapper = '''
mkdir -p "$ARTIFACT_LOCAL_DIR"

# Meson runs `sh -c SCRIPT ARG0 ARG1 ...` where ARG0 becomes `$0`.
# Keep compatibility with both forms by preferring `$1` and falling back
# to `$0` when no positional argument is supplied.
if test -n "${1:-}"; then
  test_script=$1
  shift
else
  test_script=$0
fi

case "${test_script##*.}" in
  py|rb|pl|php)
    ;;
  *)
    exec sh "$test_script" "$@"
    ;;
esac

venv_progress() {
  venv_name=$1
  shift
  printf '%s\n' "# [venv:${venv_name}] $*" >&2
}

if test "${test_script##*.}" = "py"; then
  # Resolve Python test venv once per meson test invocation. The first
  # Python TAP worker initializes the venv and writes exported variables,
  # while other workers wait for the shared result file.
  venv_state_dir="$MESON_BUILD_ROOT/tests"
  venv_run_id="${MESON_TEST_RUN_ID:-$PPID}"
  venv_exports_file="$venv_state_dir/.python-test-venv.$venv_run_id.exports"
  venv_lock_dir="$venv_state_dir/.python-test-venv.$venv_run_id.lock"

  if test ! -f "$venv_exports_file"; then
    if mkdir "$venv_lock_dir" 2>/dev/null; then
      venv_progress python "acquired lock; resolving test venv"
      venv_exports="$($TOP_SRCDIR/build-aux/resolve-python-test-venv.sh \
        "${ENABLE_PYTHON:-0}" "${SIXEL_CONFIGURED_PYTHON:-}" \
        "$MESON_BUILD_ROOT/python/dist" \
        "$MESON_BUILD_ROOT/tests/.python-test-venv")"
      printf '%s\n' "$venv_exports" > "$venv_exports_file"
      rmdir "$venv_lock_dir"
      venv_progress python "venv resolve completed"
    else
      venv_progress python "waiting for another worker to finish shared venv setup"
      wait_count=0
      while test ! -f "$venv_exports_file"; do
        wait_count=$((wait_count + 1))
        if test "$wait_count" -eq 1 || test $((wait_count % 100)) -eq 0; then
          venv_progress python "still waiting for shared venv exports (${wait_count}/600)"
        fi
        if test "$wait_count" -gt 600; then
          venv_progress python "timeout waiting for shared venv exports; marking interpreter unavailable"
          printf '%s\n' 'SIXEL_TEST_PYTHON=' > "$venv_exports_file"
          break
        fi
        sleep 0.05
      done
      if test -f "$venv_exports_file"; then
        venv_progress python "detected shared venv exports file"
      fi
    fi
  fi

  # shellcheck disable=SC1090
  . "$venv_exports_file"

  if test -z "${SIXEL_TEST_PYTHON:-}"; then
    printf '%s\n' '1..0 # SKIP python test interpreter is unavailable'
    exit 0
  fi

  exec "$SIXEL_TEST_PYTHON" "$test_script" "$@"
fi

if test "${test_script##*.}" = "rb"; then
  ruby_venv_state_dir="$MESON_BUILD_ROOT/tests"
  ruby_venv_run_id="${MESON_TEST_RUN_ID:-$PPID}"
  ruby_venv_exports_file="$ruby_venv_state_dir/.ruby-test-venv.$ruby_venv_run_id.exports"
  ruby_venv_lock_dir="$ruby_venv_state_dir/.ruby-test-venv.$ruby_venv_run_id.lock"

  if test ! -f "$ruby_venv_exports_file"; then
    if mkdir "$ruby_venv_lock_dir" 2>/dev/null; then
      venv_progress ruby "acquired lock; resolving test venv"
      ruby_venv_exports="$($TOP_SRCDIR/build-aux/resolve-ruby-test-venv.sh \
        "${ENABLE_RUBY:-0}" "${SIXEL_CONFIGURED_RUBY:-}" \
        "$MESON_BUILD_ROOT/ruby/dist" \
        "$MESON_BUILD_ROOT/tests/.ruby-test-venv")"
      printf '%s\n' "$ruby_venv_exports" > "$ruby_venv_exports_file"
      rmdir "$ruby_venv_lock_dir"
      venv_progress ruby "venv resolve completed"
    else
      venv_progress ruby "waiting for another worker to finish shared venv setup"
      wait_count=0
      while test ! -f "$ruby_venv_exports_file"; do
        wait_count=$((wait_count + 1))
        if test "$wait_count" -eq 1 || test $((wait_count % 100)) -eq 0; then
          venv_progress ruby "still waiting for shared venv exports (${wait_count}/600)"
        fi
        if test "$wait_count" -gt 600; then
          venv_progress ruby "timeout waiting for shared venv exports; marking interpreter unavailable"
          printf '%s\n' 'SIXEL_TEST_RUBY=' > "$ruby_venv_exports_file"
          printf '%s\n' 'SIXEL_TEST_RUBY_GEM_HOME=' >> "$ruby_venv_exports_file"
          printf '%s\n' 'SIXEL_TEST_RUBYLIB=' >> "$ruby_venv_exports_file"
          break
        fi
        sleep 0.05
      done
      if test -f "$ruby_venv_exports_file"; then
        venv_progress ruby "detected shared venv exports file"
      fi
    fi
  fi

  # shellcheck disable=SC1090
  . "$ruby_venv_exports_file"

  if test -z "${SIXEL_TEST_RUBY:-}"; then
    printf '%s\n' '1..0 # SKIP ruby test interpreter is unavailable'
    exit 0
  fi

  if test -n "${SIXEL_TEST_RUBY_GEM_HOME:-}"; then
    if test -n "${SIXEL_TEST_RUBYLIB:-}"; then
      if test -n "${RUBYLIB:-}"; then
        RUBYLIB="${SIXEL_TEST_RUBYLIB}:${RUBYLIB}" export RUBYLIB
      else
        RUBYLIB="${SIXEL_TEST_RUBYLIB}" export RUBYLIB
      fi
    fi
    GEM_HOME="$SIXEL_TEST_RUBY_GEM_HOME" export GEM_HOME
    GEM_PATH="$SIXEL_TEST_RUBY_GEM_HOME" export GEM_PATH
  fi

  exec "$SIXEL_TEST_RUBY" "$test_script" "$@"
fi

if test "${test_script##*.}" = "php"; then
  php_venv_state_dir="$MESON_BUILD_ROOT/tests"
  php_venv_run_id="${MESON_TEST_RUN_ID:-$PPID}"
  php_venv_exports_file="$php_venv_state_dir/.php-test-venv.$php_venv_run_id.exports"
  php_venv_lock_dir="$php_venv_state_dir/.php-test-venv.$php_venv_run_id.lock"

  if test ! -f "$php_venv_exports_file"; then
    if mkdir "$php_venv_lock_dir" 2>/dev/null; then
      venv_progress php "acquired lock; resolving test venv"
      php_venv_exports="$($TOP_SRCDIR/build-aux/resolve-php-test-venv.sh \
        "${ENABLE_PHP:-0}" "${SIXEL_CONFIGURED_PHP:-}" \
        "$MESON_BUILD_ROOT/php/dist" \
        "$MESON_BUILD_ROOT/tests/.php-test-venv")"
      printf '%s\n' "$php_venv_exports" > "$php_venv_exports_file"
      rmdir "$php_venv_lock_dir"
      venv_progress php "venv resolve completed"
    else
      venv_progress php "waiting for another worker to finish shared venv setup"
      wait_count=0
      while test ! -f "$php_venv_exports_file"; do
        wait_count=$((wait_count + 1))
        if test "$wait_count" -eq 1 || test $((wait_count % 100)) -eq 0; then
          venv_progress php "still waiting for shared venv exports (${wait_count}/600)"
        fi
        if test "$wait_count" -gt 600; then
          venv_progress php "timeout waiting for shared venv exports; marking interpreter unavailable"
          printf '%s\n' 'SIXEL_TEST_PHP=' > "$php_venv_exports_file"
          printf '%s\n' 'SIXEL_TEST_PHP_BINDING_ROOT=' >> "$php_venv_exports_file"
          printf '%s\n' 'SIXEL_TEST_PHP_LIBDIR=' >> "$php_venv_exports_file"
          printf '%s\n' 'SIXEL_TEST_PHP_LIBPATH=' >> "$php_venv_exports_file"
          break
        fi
        sleep 0.05
      done
      if test -f "$php_venv_exports_file"; then
        venv_progress php "detected shared venv exports file"
      fi
    fi
  fi

  # shellcheck disable=SC1090
  . "$php_venv_exports_file"

  if test -z "${SIXEL_TEST_PHP:-}"; then
    if test "${ENABLE_PHP:-0}" = "1"; then
      printf '%s\n' '1..1'
      printf '%s\n' 'not ok 1 - php venv interpreter is unavailable (system fallback disabled)'
      exit 1
    fi
    printf '%s\n' '1..0 # SKIP php test interpreter is unavailable'
    exit 0
  fi

  if test -n "${SIXEL_TEST_PHP_BINDING_ROOT:-}"; then
    SIXEL_TEST_PHP_BINDING_ROOT="${SIXEL_TEST_PHP_BINDING_ROOT}" export SIXEL_TEST_PHP_BINDING_ROOT
  fi
  if test -n "${SIXEL_TEST_PHP_LIBDIR:-}"; then
    LIBSIXEL_LIBDIR="${SIXEL_TEST_PHP_LIBDIR}" export LIBSIXEL_LIBDIR
  fi
  if test -n "${SIXEL_TEST_PHP_LIBPATH:-}"; then
    LIBSIXEL_LIBPATH="${SIXEL_TEST_PHP_LIBPATH}" export LIBSIXEL_LIBPATH
  fi

  exec "$SIXEL_TEST_PHP" "$test_script" "$@"
fi

perl_venv_state_dir="$MESON_BUILD_ROOT/tests"
perl_venv_run_id="${MESON_TEST_RUN_ID:-$PPID}"
perl_venv_exports_file="$perl_venv_state_dir/.perl-test-venv.$perl_venv_run_id.exports"
perl_venv_lock_dir="$perl_venv_state_dir/.perl-test-venv.$perl_venv_run_id.lock"

if test ! -f "$perl_venv_exports_file"; then
  if mkdir "$perl_venv_lock_dir" 2>/dev/null; then
    venv_progress perl "acquired lock; resolving test venv"
    perl_venv_exports="$($TOP_SRCDIR/build-aux/resolve-perl-test-venv.sh \
      "${ENABLE_PERL:-0}" "${SIXEL_CONFIGURED_PERL:-}" \
      "$TOP_SRCDIR/perl" "$MESON_BUILD_ROOT/src/.libs" \
      "$MESON_BUILD_ROOT/tests/.perl-test-venv")"
    printf '%s\n' "$perl_venv_exports" > "$perl_venv_exports_file"
    rmdir "$perl_venv_lock_dir"
    venv_progress perl "venv resolve completed"
  else
    venv_progress perl "waiting for another worker to finish shared venv setup"
    wait_count=0
    while test ! -f "$perl_venv_exports_file"; do
      wait_count=$((wait_count + 1))
      if test "$wait_count" -eq 1 || test $((wait_count % 100)) -eq 0; then
        venv_progress perl "still waiting for shared venv exports (${wait_count}/600)"
      fi
      if test "$wait_count" -gt 600; then
        venv_progress perl "timeout waiting for shared venv exports; marking interpreter unavailable"
        printf '%s\n' 'SIXEL_TEST_PERL=' > "$perl_venv_exports_file"
        printf '%s\n' 'SIXEL_TEST_PERL5LIB=' >> "$perl_venv_exports_file"
        printf '%s\n' 'SIXEL_TEST_PERL_LOCAL_LIB_ROOT=' >> "$perl_venv_exports_file"
        printf '%s\n' 'SIXEL_TEST_PERL_MB_OPT=' >> "$perl_venv_exports_file"
        printf '%s\n' 'SIXEL_TEST_PERL_MM_OPT=' >> "$perl_venv_exports_file"
        break
      fi
      sleep 0.05
    done
    if test -f "$perl_venv_exports_file"; then
      venv_progress perl "detected shared venv exports file"
    fi
  fi
fi

# shellcheck disable=SC1090
. "$perl_venv_exports_file"

if test -z "${SIXEL_TEST_PERL:-}"; then
  printf '%s\n' '1..0 # SKIP perl test interpreter is unavailable'
  exit 0
fi

if test -n "${SIXEL_TEST_PERL5LIB:-}"; then
  if test -n "${PERL5LIB:-}"; then
    PERL5LIB="${SIXEL_TEST_PERL5LIB}:${PERL5LIB}" export PERL5LIB
  else
    PERL5LIB="${SIXEL_TEST_PERL5LIB}" export PERL5LIB
  fi
fi
if test -n "${SIXEL_TEST_PERL_LOCAL_LIB_ROOT:-}"; then
  PERL_LOCAL_LIB_ROOT="${SIXEL_TEST_PERL_LOCAL_LIB_ROOT}" export PERL_LOCAL_LIB_ROOT
fi
if test -n "${SIXEL_TEST_PERL_MB_OPT:-}"; then
  PERL_MB_OPT="${SIXEL_TEST_PERL_MB_OPT}" export PERL_MB_OPT
fi
if test -n "${SIXEL_TEST_PERL_MM_OPT:-}"; then
  PERL_MM_OPT="${SIXEL_TEST_PERL_MM_OPT}" export PERL_MM_OPT
fi

exec "$SIXEL_TEST_PERL" "$test_script" "$@"
'''

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
  test_env.set('ARTIFACT_LOCAL_DIR', test_artifact_dir)
  test_cmd = [meson.current_source_dir() / t]
  test(t, sh,
    args: ['-c', python_tap_wrapper, 'sh'] + test_cmd,
    depends: test_deps,
    protocol: 'tap',
    env: test_env,
    timeout: 90,
    suite: ['tap'])
endforeach

add_test_setup('ci', timeout_multiplier: 1)

run_target('test-python',
  command: [sh, '-c', '''
set -eu

python_bin="$1"
source_root="$2"
build_root="$3"

if test -z "$python_bin"; then
  echo "test-python: skipped because Python test interpreter is unavailable"
  exit 0
fi

python_venv_exports="$($source_root/build-aux/resolve-python-test-venv.sh 1 "$python_bin" "$build_root/python/dist" "$build_root/tests/.python-test-venv")"
eval "$python_venv_exports"
if test -z "${SIXEL_TEST_PYTHON:-}"; then
  echo "test-python: skipped because Python test virtualenv is unavailable"
  exit 0
fi

"$SIXEL_TEST_PYTHON" -m pip install --disable-pip-version-check --quiet pytest coverage
mkdir -p "$build_root/tests/_artifacts/python/pytest"
ARTIFACT_LOCAL_DIR="$build_root/tests/_artifacts/python/pytest" TOP_SRCDIR="$source_root" TOP_BUILDDIR="$build_root" LIBSIXEL_LIBDIR="$build_root/src" "$SIXEL_TEST_PYTHON" -m coverage run --source=libsixel_wheel -m pytest -q -o python_files='[0-9][0-9][0-9][0-9]_*.py' "$source_root/tests/bindings/python"
"$SIXEL_TEST_PYTHON" -m coverage lcov -o "$build_root/python-coverage.info"
python_cov_tmp="$build_root/python-coverage.info.tmp"
sed -E 's#^SF:.*site-packages/libsixel_wheel/#SF:python/libsixel/#' "$build_root/python-coverage.info" > "$python_cov_tmp"
mv "$python_cov_tmp" "$build_root/python-coverage.info"
echo "python coverage exported to $build_root/python-coverage.info"
''',
            'sh',
            python_for_tests_path,
            meson.project_source_root(),
            meson.project_build_root()],
  depends: test_deps)

run_target('test-ruby',
  command: [sh, '-c', '''
set -eu

ruby_bin="$1"
source_root="$2"
build_root="$3"

if test -z "$ruby_bin"; then
  echo "test-ruby: skipped because Ruby test interpreter is unavailable"
  exit 0
fi

ruby_venv_exports="$($source_root/build-aux/resolve-ruby-test-venv.sh 1 "$ruby_bin" "$build_root/ruby/dist" "$build_root/tests/.ruby-test-venv")"
eval "$ruby_venv_exports"
if test -z "${SIXEL_TEST_RUBY:-}"; then
  echo "test-ruby: skipped because Ruby test interpreter is unavailable"
  exit 0
fi

if test -n "${SIXEL_TEST_RUBY_GEM_HOME:-}"; then
  GEM_HOME="$SIXEL_TEST_RUBY_GEM_HOME" export GEM_HOME
  GEM_PATH="$SIXEL_TEST_RUBY_GEM_HOME" export GEM_PATH
fi
if test -n "${SIXEL_TEST_RUBYLIB:-}"; then
  if test -n "${RUBYLIB:-}"; then
    RUBYLIB="$SIXEL_TEST_RUBYLIB:$RUBYLIB" export RUBYLIB
  else
    RUBYLIB="$SIXEL_TEST_RUBYLIB" export RUBYLIB
  fi
fi

artifact_root="$build_root/tests/_artifacts/ruby/tap"
coverage_root="$build_root/tests/_artifacts/ruby/coverage"
coverage_runner="$source_root/tests/bindings/ruby/_coverage_runner.rb"
coverage_converter="$source_root/tests/bindings/ruby/_coverage_to_lcov.rb"
lcov_path="$build_root/ruby-coverage.info"
mkdir -p "$artifact_root"
rm -rf "$coverage_root"
mkdir -p "$coverage_root"
status=0

for test_path in $(find "$source_root/tests/bindings/ruby" -maxdepth 1 -type f -name '[0-9][0-9][0-9][0-9]_*.rb' | LC_ALL=C sort); do
  test_name=$(basename "$test_path" .rb)
  artifact_dir="$artifact_root/$test_name"
  log_path="$artifact_root/$test_name.log"
  coverage_path="$coverage_root/$test_name.marshal"
  mkdir -p "$artifact_dir"
  echo "==> $test_name.rb"
  if ARTIFACT_LOCAL_DIR="$artifact_dir" TOP_SRCDIR="$source_root" TOP_BUILDDIR="$build_root" LIBSIXEL_LIBDIR="$build_root/src" "$SIXEL_TEST_RUBY" "$coverage_runner" "$test_path" "$coverage_path" >"$log_path" 2>&1; then
    if grep -Eq '^not ok([[:space:]]|$)' "$log_path"; then
      echo "not ok - $test_name.rb (TAP not ok found)"
      sed -n '1,120p' "$log_path"
      status=1
    fi
  else
    echo "not ok - $test_name.rb (ruby exit code != 0)"
    sed -n '1,120p' "$log_path"
    status=1
  fi
done

"$SIXEL_TEST_RUBY" "$coverage_converter" "$coverage_root" "$lcov_path" "$source_root" || status=1
echo "ruby coverage exported to $lcov_path"

if test "$status" -ne 0; then
  exit "$status"
fi

echo "ruby TAP logs exported to $artifact_root"
''',
            'sh',
            ruby_for_tests_path,
            meson.project_source_root(),
            meson.project_build_root()],
  depends: test_deps)

staticcheck_cases = [
  [
    'staticcheck-actionlint',
    [meson.current_source_dir() / '_static/sh/staticcheck-actionlint.sh',
      meson.project_source_root(),
      actionlint_prog.found() ? actionlint_prog.full_path() : ''],
  ],
  [
    'staticcheck-shellcheck',
    [meson.current_source_dir() / '_static/sh/staticcheck-shellcheck.sh',
      meson.current_source_dir() / '_static/sh/shellcheck.sh',
      meson.project_source_root(),
      shellcheck_prog.found() ? shellcheck_prog.full_path() : ''],
  ],
  [
    'staticcheck-codespell',
    [meson.current_source_dir() / '_static/sh/staticcheck-codespell.sh',
      meson.project_source_root(),
      codespell_prog.found() ? codespell_prog.full_path() : ''],
  ],
  [
    'staticcheck-python-compileall',
    [meson.current_source_dir() / '_static/sh/staticcheck-python-compileall.sh',
      meson.project_source_root(),
      python_for_tests_path],
  ],
  [
    'staticcheck-build-doc-configure-help',
    [meson.current_source_dir() / '_static/sh/staticcheck-build-doc-configure-help.sh',
      meson.project_source_root()],
  ],
  [
    'staticcheck-build-doc-meson-options',
    [meson.current_source_dir() / '_static/sh/staticcheck-build-doc-meson-options.sh',
      meson.project_source_root()],
  ],
]

foreach static_case : staticcheck_cases
  static_cmd = static_case[1]
  run_target(static_case[0], command: [sh] + static_cmd)
endforeach

run_target('staticcheck',
  command: [
    sh,
    '-c',
    'set -eu; "$1" "$2" "$3"; "$4" "$5" "$6" "$7"; "$8" "$9" "${10}"; "${11}" "${12}" "${13}"; "${14}" "${15}"; "${16}" "${17}"',
    'sh',
    staticcheck_cases[0][1][0],
    staticcheck_cases[0][1][1],
    staticcheck_cases[0][1][2],
    staticcheck_cases[1][1][0],
    staticcheck_cases[1][1][1],
    staticcheck_cases[1][1][2],
    staticcheck_cases[1][1][3],
    staticcheck_cases[2][1][0],
    staticcheck_cases[2][1][1],
    staticcheck_cases[2][1][2],
    staticcheck_cases[3][1][0],
    staticcheck_cases[3][1][1],
    staticcheck_cases[3][1][2],
    staticcheck_cases[4][1][0],
    staticcheck_cases[4][1][1],
    staticcheck_cases[5][1][0],
    staticcheck_cases[5][1][1],
  ])
