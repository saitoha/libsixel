#!/bin/sh

mkdir -p "$ARTIFACT_LOCAL_DIR"

prepare_psb_large_fixtures_once() {
  state_dir="$MESON_BUILD_ROOT/tests"
  run_id="${MESON_TEST_RUN_ID:-$PPID}"
  done_file="$state_dir/.psb-large-fixtures.$run_id.done"
  lock_dir="$state_dir/.psb-large-fixtures.$run_id.lock"
  prepare_script="$TOP_SRCDIR/tests/_static/sh/prepare-psb-large-fixtures.sh"

  mkdir -p "$state_dir"

  if test -f "$done_file"; then
    return 0
  fi

  if mkdir "$lock_dir" 2>/dev/null; then
    if ! sh "$prepare_script" "$TOP_SRCDIR"; then
      rmdir "$lock_dir"
      return 1
    fi
    : > "$done_file"
    rmdir "$lock_dir"
    return 0
  fi

  wait_count=0
  while test ! -f "$done_file"; do
    wait_count=$((wait_count + 1))
    if test "$wait_count" -gt 600; then
      printf '%s\n' "error: timeout waiting for PSB large fixture preparation" >&2
      return 1
    fi
    sleep 0.05
  done
}

if ! prepare_psb_large_fixtures_once; then
  exit 1
fi

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
      venv_exports="$("$TOP_SRCDIR"/build-aux/resolve-python-test-venv.sh \
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
      ruby_venv_exports="$("$TOP_SRCDIR"/build-aux/resolve-ruby-test-venv.sh \
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
      php_venv_exports="$("$TOP_SRCDIR"/build-aux/resolve-php-test-venv.sh \
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
    perl_venv_exports="$("$TOP_SRCDIR"/build-aux/resolve-perl-test-venv.sh \
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
