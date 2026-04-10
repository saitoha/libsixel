#!/bin/sh
# Profile execution time for gd optional fallback TAP tests.

set -eu

src_root=${1:-$(cd "$(dirname "$0")/../../.." && pwd)}
build_root=${2:-$src_root}

TOP_SRCDIR=${TOP_SRCDIR:-$src_root}
TOP_BUILDDIR=${TOP_BUILDDIR:-$build_root}
ARTIFACT_ROOT=${ARTIFACT_ROOT:-$build_root/tests/_artifacts}
ARTIFACT_LOCAL_DIR=${ARTIFACT_LOCAL_DIR:-$ARTIFACT_ROOT/loader-gd-optional-profile}
export TOP_SRCDIR TOP_BUILDDIR ARTIFACT_ROOT ARTIFACT_LOCAL_DIR

set -a
eval "$(/bin/sh "$src_root"/build-aux/resolve-config-h-exports.sh \
    "$build_root/config.h" exports)"
set +a

if test -f "$build_root/tests/test_runner"; then
    TEST_RUNNER_PATH="$build_root/tests/test_runner"
    export TEST_RUNNER_PATH
fi
if test -f "$build_root/converters/img2sixel"; then
    IMG2SIXEL_PATH="$build_root/converters/img2sixel"
    export IMG2SIXEL_PATH
fi
if test -f "$build_root/converters/sixel2png"; then
    SIXEL2PNG_PATH="$build_root/converters/sixel2png"
    export SIXEL2PNG_PATH
fi
if test -f "$build_root/assessment/lsqa"; then
    LSQA_PATH="$build_root/assessment/lsqa"
    export LSQA_PATH
fi
if test -d "${LIBSIXEL_LIBDIR-}"; then
    DYLD_LIBRARY_PATH="$LIBSIXEL_LIBDIR${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
    LD_LIBRARY_PATH="$LIBSIXEL_LIBDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export DYLD_LIBRARY_PATH LD_LIBRARY_PATH
fi

time_cmd="/usr/bin/time"
test -x "$time_cmd" || time_cmd="time"

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/run-loader-gd-optional-profile.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

printf '%s\n' 'test\tresult\treal_seconds'

for test_name in \
    0111_loader_gd_wbmp_unsupported_fallback_any_loader.t \
    0112_loader_gd_gd2_unsupported_fallback_any_loader.t \
    0118_loader_gd_tga_unsupported_stdin_fallback_builtin.t \
    0119_loader_gd_webp_unsupported_stdin_fallback_libwebp.t \
    0122_loader_gd_wbmp_unsupported_stdin_fallback_any_loader.t \
    0123_loader_gd_gd2_unsupported_stdin_fallback_any_loader.t
do
    test_path="$src_root/tests/loader/gd/$test_name"
    log_path="$tmpdir/$test_name.log"

    run_rc=0
    (
        cd "$build_root/tests"
        "$time_cmd" -p "$test_path"
    ) >"$log_path" 2>&1 || run_rc=$?

    real_seconds=$(awk '/^real[[:space:]]+[0-9.]+$/ { print $2; exit }' \
        "$log_path")
    test -n "$real_seconds" || real_seconds="n/a"

    if test "$run_rc" -ne 0; then
        result="FAIL"
    elif awk '/^1\.\.0[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp]/ { found=1 }
              /^ok[[:space:]]+[0-9]+[[:space:]]*#[[:space:]]*[Ss][Kk][Ii][Pp]/ { found=1 }
              END { exit found ? 0 : 1 }' "$log_path"; then
        result="SKIP"
    elif awk '/^not ok([[:space:]]|$)/ { found=1 }
              /^Bail out!/ { found=1 }
              END { exit found ? 0 : 1 }' "$log_path"; then
        result="FAIL"
    else
        result="PASS"
    fi

    printf '%s\t%s\t%s\n' "$test_name" "$result" "$real_seconds"
done

exit 0
