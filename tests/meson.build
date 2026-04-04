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
  # Expose the configured C compiler ID to TAP scripts so platform quirks
  # can be gated without conflating all Windows toolchains.
  ['SIXEL_TEST_C_COMPILER_ID', cc.get_id()],
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

# Keep large generated fixtures out of the source tree and make Meson test
# inputs match autotools by materializing them under TOP_BUILDDIR.
run_command(
  sh,
  meson.project_source_root() / 'tests/_static/sh/prepare-psb-large-fixtures.sh',
  meson.project_source_root(),
  meson.project_build_root(),
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
  '-prune -o -type f \\( -name "*.c" -o -name "*.h" \\) ' +
  '! -name "*.inc.c" -print | ' +
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

python_tap_wrapper_file = configure_file(
  input: 'meson-python-tap-wrapper.sh',
  output: 'meson-python-tap-wrapper.sh',
  copy: true,
)

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
  test_artifact_leaf = test_base
  if test_artifact_leaf.substring(43) != ''
    test_artifact_leaf = '@0@...@1@'.format(
      test_artifact_leaf.substring(0, 20),
      test_artifact_leaf.substring(-20),
    )
  endif
  test_artifact_dir = artifact_root / test_category / test_artifact_leaf
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
    args: [python_tap_wrapper_file] + test_cmd,
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
test_seq=0

for test_path in $(find "$source_root/tests/bindings/ruby" -maxdepth 1 -type f -name '[0-9][0-9][0-9][0-9]_*.rb' | LC_ALL=C sort); do
  test_name="${test_path##*/}"
  test_name="${test_name%.rb}"
  test_seq=$((test_seq + 1))
  test_id="${test_name%%_*}"
  case "$test_id" in
    [0-9][0-9][0-9][0-9]) ;;
    *) test_id=t ;;
  esac
  artifact_dir="$artifact_root/$test_id-$test_seq"
  log_path="$artifact_root/$test_id-$test_seq.log"
  coverage_path="$coverage_root/$test_id-$test_seq.marshal"
  mkdir -p "$artifact_dir"
  echo "==> $test_name.rb"
  if ARTIFACT_LOCAL_DIR="$artifact_dir" TOP_SRCDIR="$source_root" TOP_BUILDDIR="$build_root" LIBSIXEL_LIBDIR="$build_root/src" "$SIXEL_TEST_RUBY" --disable-gems "$coverage_runner" "$test_path" "$coverage_path" >"$log_path" 2>&1; then
    tap_not_ok=0
    while IFS= read -r tap_line || test -n "$tap_line"; do
      case "$tap_line" in
        "not ok"|"not ok "*)
          tap_not_ok=1
          break
          ;;
      esac
    done < "$log_path"
    if test "$tap_not_ok" -ne 0; then
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

"$SIXEL_TEST_RUBY" --disable-gems "$coverage_converter" "$coverage_root" "$lcov_path" "$source_root" || status=1
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

run_target('test-perl',
  command: [sh, '-c', '''
set -eu

perl_bin="$1"
source_root="$2"
build_root="$3"

if test -z "$perl_bin"; then
  echo "test-perl: skipped because Perl test interpreter is unavailable"
  exit 0
fi

perl_venv_exports="$($source_root/build-aux/resolve-perl-test-venv.sh 1 "$perl_bin" "$source_root/perl" "$build_root/src" "$build_root/tests/.perl-test-venv")"
eval "$perl_venv_exports"
if test -z "${SIXEL_TEST_PERL:-}"; then
  echo "test-perl: skipped because Perl test interpreter is unavailable"
  exit 0
fi
if test -z "${SIXEL_TEST_PERL_CPANM:-}"; then
  echo "test-perl: skipped because cpanm is unavailable for Perl test venv"
  exit 0
fi
if test -n "${SIXEL_TEST_PERL5LIB:-}"; then
  if test -n "${PERL5LIB:-}"; then
    PERL5LIB="$SIXEL_TEST_PERL5LIB:$PERL5LIB" export PERL5LIB
  else
    PERL5LIB="$SIXEL_TEST_PERL5LIB" export PERL5LIB
  fi
fi
if test -n "${SIXEL_TEST_PERL_LOCAL_LIB_ROOT:-}"; then
  PERL_LOCAL_LIB_ROOT="$SIXEL_TEST_PERL_LOCAL_LIB_ROOT" export PERL_LOCAL_LIB_ROOT
fi
if test -n "${SIXEL_TEST_PERL_MB_OPT:-}"; then
  PERL_MB_OPT="$SIXEL_TEST_PERL_MB_OPT" export PERL_MB_OPT
fi
if test -n "${SIXEL_TEST_PERL_MM_OPT:-}"; then
  PERL_MM_OPT="$SIXEL_TEST_PERL_MM_OPT" export PERL_MM_OPT
fi
if test -n "${SIXEL_TEST_PERL_LOCAL_LIB_ROOT:-}"; then
  PERL_CPANM_HOME="$SIXEL_TEST_PERL_LOCAL_LIB_ROOT/.cpanm" export PERL_CPANM_HOME
  mkdir -p "$PERL_CPANM_HOME"
fi

"$SIXEL_TEST_PERL" "$SIXEL_TEST_PERL_CPANM" --quiet --notest --skip-satisfied --local-lib-contained "$SIXEL_TEST_PERL_LOCAL_LIB_ROOT" Devel::Cover
cover_bin="$SIXEL_TEST_PERL_LOCAL_LIB_ROOT/bin/cover"
if test ! -x "$cover_bin"; then
  echo "test-perl: expected cover executable was not found at $cover_bin" >&2
  exit 1
fi

artifact_root="$build_root/tests/_artifacts/perl/tap"
coverage_root="$build_root/tests/_artifacts/perl/coverage"
coverage_converter="$source_root/tests/bindings/perl/_coverage_to_lcov.pl"
lcov_path="$build_root/perl-coverage.info"
mkdir -p "$artifact_root"
rm -rf "$coverage_root"
mkdir -p "$coverage_root"
status=0
test_seq=0

for test_path in $(find "$source_root/tests/bindings/perl" -maxdepth 1 -type f -name '[0-9][0-9][0-9][0-9]_*.pl' | LC_ALL=C sort); do
  test_name=$(basename "$test_path" .pl)
  test_seq=$((test_seq + 1))
  test_id="${test_name%%_*}"
  case "$test_id" in
    [0-9][0-9][0-9][0-9]) ;;
    *) test_id=t ;;
  esac
  artifact_dir="$artifact_root/$test_id-$test_seq"
  log_path="$artifact_root/$test_id-$test_seq.log"
  mkdir -p "$artifact_dir"
  echo "==> $test_name.pl"
  if ARTIFACT_LOCAL_DIR="$artifact_dir" TOP_SRCDIR="$source_root" TOP_BUILDDIR="$build_root" LIBSIXEL_LIBDIR="$build_root/src" "$SIXEL_TEST_PERL" -MDevel::Cover=-db,$coverage_root,-silent,1 "$test_path" >"$log_path" 2>&1; then
    if grep -Eq '^not ok([[:space:]]|$)' "$log_path"; then
      echo "not ok - $test_name.pl (TAP not ok found)"
      sed -n '1,120p' "$log_path"
      status=1
    fi
  else
    echo "not ok - $test_name.pl (perl exit code != 0)"
    sed -n '1,120p' "$log_path"
    status=1
  fi
done

"$cover_bin" "$coverage_root" -silent 1 > /dev/null 2>&1 || status=1
"$SIXEL_TEST_PERL" "$coverage_converter" "$coverage_root" "$lcov_path" "$source_root" "$SIXEL_TEST_PERL_LOCAL_LIB_ROOT" || status=1
echo "perl coverage exported to $lcov_path"

if test "$status" -ne 0; then
  exit "$status"
fi

echo "perl TAP logs exported to $artifact_root"
''',
            'sh',
            perl_for_tests_path,
            meson.project_source_root(),
            meson.project_build_root()],
  depends: test_deps)

run_target('test-php',
  command: [sh, '-c', '''
set -eu

php_bin="$1"
source_root="$2"
build_root="$3"

if test -z "$php_bin"; then
  echo "test-php: skipped because PHP test interpreter is unavailable"
  exit 0
fi

php_venv_exports="$($source_root/build-aux/resolve-php-test-venv.sh 1 "$php_bin" "$build_root/php/dist" "$build_root/tests/.php-test-venv")"
eval "$php_venv_exports"
if test -z "${SIXEL_TEST_PHP:-}"; then
  echo "test-php: failed because venv PHP interpreter is unavailable (system fallback is disabled)" >&2
  exit 1
fi
if test -z "${SIXEL_TEST_PHPDBG:-}"; then
  echo "test-php: failed because phpdbg is unavailable in the PHP test venv" >&2
  exit 1
fi
if test -z "${SIXEL_TEST_PHP_BINDING_ROOT:-}" || test -z "${SIXEL_TEST_PHP_LIBPATH:-}"; then
  echo "test-php: failed because packaged PHP binding payload is unavailable in the venv" >&2
  exit 1
fi

status=0
artifact_root="$build_root/tests/_artifacts/php/tap"
coverage_root="$build_root/tests/_artifacts/php/coverage"
coverage_runner="$source_root/tests/bindings/php/_coverage_runner.php"
coverage_converter="$source_root/tests/bindings/php/_coverage_to_lcov.php"
lcov_path="$build_root/php-coverage.info"
mkdir -p "$artifact_root"
rm -rf "$coverage_root"
mkdir -p "$coverage_root"
test_seq=0

for test_path in $(find "$source_root/tests/bindings/php" -maxdepth 1 -type f -name '[0-9][0-9][0-9][0-9]_*.php' | LC_ALL=C sort); do
  test_name=$(basename "$test_path" .php)
  test_seq=$((test_seq + 1))
  test_id="${test_name%%_*}"
  case "$test_id" in
    [0-9][0-9][0-9][0-9]) ;;
    *) test_id=t ;;
  esac
  artifact_dir="$artifact_root/$test_id-$test_seq"
  log_path="$artifact_root/$test_id-$test_seq.log"
  coverage_path="$coverage_root/$test_id-$test_seq.json"
  mkdir -p "$artifact_dir"
  echo "==> $test_name.php"
  if ARTIFACT_LOCAL_DIR="$artifact_dir" TOP_SRCDIR="$source_root" TOP_BUILDDIR="$build_root" LIBSIXEL_LIBDIR="$SIXEL_TEST_PHP_LIBDIR" LIBSIXEL_LIBPATH="$SIXEL_TEST_PHP_LIBPATH" SIXEL_TEST_PHP_BINDING_ROOT="$SIXEL_TEST_PHP_BINDING_ROOT" "$SIXEL_TEST_PHPDBG" -d ffi.enable=true -qrr "$coverage_runner" "$test_path" "$coverage_path" >"$log_path" 2>&1; then
    if grep -Eq '^not ok([[:space:]]|$)' "$log_path"; then
      echo "not ok - $test_name.php (TAP not ok found)"
      sed -n '1,120p' "$log_path"
      status=1
    fi
  else
    echo "not ok - $test_name.php (phpdbg exit code != 0)"
    sed -n '1,120p' "$log_path"
    status=1
  fi
done

"$SIXEL_TEST_PHP" "$coverage_converter" "$coverage_root" "$lcov_path" "$source_root" || status=1
echo "php coverage exported to $lcov_path"

if test "$status" -ne 0; then
  exit "$status"
fi

echo "php TAP logs exported to $artifact_root"
''',
            'sh',
            php_for_tests_path,
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
    'staticcheck-private-includes',
    [meson.current_source_dir() / '_static/sh/staticcheck-private-includes.sh',
      meson.project_source_root(),
      python_for_tests_path],
  ],
  [
    'staticcheck-makefile-recipes',
    [meson.current_source_dir() / '_static/sh/staticcheck-makefile-recipes.sh',
      meson.project_source_root()],
  ],
  [
    'staticcheck-binding-loader-setopt-sync',
    [meson.current_source_dir() / '_static/sh/staticcheck-binding-loader-setopt-sync.sh',
      meson.project_source_root()],
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
  [
    'staticcheck-docs-envvars-help-table',
    [meson.current_source_dir() / '_static/sh/staticcheck-docs-envvars-help-table.sh',
      meson.project_source_root()],
  ],
  [
    'staticcheck-c-header-modeline',
    [meson.current_source_dir() / '_static/sh/staticcheck-c-header-modeline.sh',
      meson.project_source_root()],
  ],
  [
    'staticcheck-test-plan-single',
    [meson.current_source_dir() / '_static/sh/staticcheck-test-plan-single.sh',
      meson.project_source_root()],
  ],
  [
    'staticcheck-artifact-local-dir-mkdir',
    [meson.current_source_dir() / '_static/sh/staticcheck-artifact-local-dir-mkdir.sh',
      meson.project_source_root()],
  ],
  [
    'staticcheck-test-runner-dllexport',
    [meson.current_source_dir() / '_static/sh/staticcheck-test-runner-dllexport.sh',
      meson.project_source_root()],
  ],
  [
    'staticcheck-test-no-grep-awk',
    [meson.current_source_dir() / '_static/sh/staticcheck-test-no-grep-awk.sh',
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
    'set -eu; "$1" "$2" "$3"; "$4" "$5" "$6" "$7"; "$8" "$9" "${10}"; "${11}" "${12}" "${13}"; "${14}" "${15}" "${16}"; "${17}" "${18}"; "${19}" "${20}"; "${21}" "${22}"; "${23}" "${24}"; "${25}" "${26}"; "${27}" "${28}"; "${29}" "${30}"; "${31}" "${32}"; "${33}" "${34}"; "${35}" "${36}"',
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
    staticcheck_cases[4][1][2],
    staticcheck_cases[5][1][0],
    staticcheck_cases[5][1][1],
    staticcheck_cases[6][1][0],
    staticcheck_cases[6][1][1],
    staticcheck_cases[7][1][0],
    staticcheck_cases[7][1][1],
    staticcheck_cases[8][1][0],
    staticcheck_cases[8][1][1],
    staticcheck_cases[9][1][0],
    staticcheck_cases[9][1][1],
    staticcheck_cases[10][1][0],
    staticcheck_cases[10][1][1],
    staticcheck_cases[11][1][0],
    staticcheck_cases[11][1][1],
    staticcheck_cases[12][1][0],
    staticcheck_cases[12][1][1],
    staticcheck_cases[13][1][0],
    staticcheck_cases[13][1][1],
    staticcheck_cases[14][1][0],
    staticcheck_cases[14][1][1],
  ])
