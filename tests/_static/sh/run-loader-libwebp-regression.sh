#!/bin/sh
# Run all loader/libwebp TAP tests with a stable local environment.

set -eu

src_root=${1:-$(cd "$(dirname "$0")/../../.." && pwd)}
build_root=${2:-$src_root}
run_staticcheck=${3:-${RUN_STATICCHECK:-0}}

set -- "$src_root"/tests/loader/libwebp/*.t
test -f "$1" || {
    printf '%s\n' 'no loader/libwebp TAP tests were found' >&2
    exit 1
}

TOP_SRCDIR=${TOP_SRCDIR:-$src_root}
TOP_BUILDDIR=${TOP_BUILDDIR:-$build_root}
ARTIFACT_ROOT=${ARTIFACT_ROOT:-$build_root/tests/_artifacts}
ARTIFACT_LOCAL_DIR=${ARTIFACT_LOCAL_DIR:-$ARTIFACT_ROOT/loader-libwebp}
export TOP_SRCDIR TOP_BUILDDIR ARTIFACT_ROOT
export ARTIFACT_LOCAL_DIR
mkdir -p "$ARTIFACT_ROOT"

set -a
eval "$(/bin/sh "$src_root"/build-aux/resolve-test-tool-paths.sh \
    "$build_root" \
    "$src_root/tests/check-test-exports.list")"
eval "$(/bin/sh "$src_root"/build-aux/resolve-config-h-exports.sh \
    "$build_root/config.h" exports)"
set +a

missing_exports=
for required_export in IMG2SIXEL_PATH SIXEL2PNG_PATH LSQA_PATH TEST_RUNNER_PATH LIBSIXEL_LIBDIR; do
    required_value=
    eval "required_value=\${$required_export-}"
    case "$required_value" in
        '')
            missing_exports="$missing_exports $required_export"
            ;;
        *)
            ;;
    esac
done
case "${missing_exports}" in
    '')
        ;;
    *)
        printf 'missing required test export(s):%s\n' "$missing_exports" >&2
        exit 1
        ;;
esac

# Prefer libtool wrappers to avoid accidental linkage to an installed libsixel.
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
if test -f "$build_root/tests/test_runner"; then
    TEST_RUNNER_PATH="$build_root/tests/test_runner"
    export TEST_RUNNER_PATH
fi

if test -d "$LIBSIXEL_LIBDIR"; then
    DYLD_LIBRARY_PATH="$LIBSIXEL_LIBDIR${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
    LD_LIBRARY_PATH="$LIBSIXEL_LIBDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export DYLD_LIBRARY_PATH LD_LIBRARY_PATH
fi

tmp_root=$(mktemp -d "${TMPDIR:-/tmp}/run-loader-libwebp-regression.XXXXXX")
# shellcheck disable=SC2329
cleanup() {
    rm -rf "$tmp_root"
}
trap cleanup EXIT HUP INT TERM

status=0
total=0
failed=0

for test_path in $(printf '%s\n' "$@" | LC_ALL=C sort); do
    total=$((total + 1))
    log_path="$tmp_root/$(basename "$test_path").log"
    case "$test_path" in
        "$src_root"/*)
            label=${test_path#"$src_root"/}
            ;;
        *)
            label=$test_path
            ;;
    esac

    ARTIFACT_LOCAL_DIR="$ARTIFACT_ROOT/$(basename "$test_path" .t)"
    export ARTIFACT_LOCAL_DIR
    mkdir -p "$ARTIFACT_LOCAL_DIR"

    test_status=0
    (
        cd "$build_root/tests"
        "$test_path"
    ) >"$log_path" 2>&1 || test_status=$?
    case "$test_status" in
        0)
            ;;
        *)
            failed=$((failed + 1))
            status=1
            printf 'FAIL %s (exit %s)\n' "$label" "$test_status" >&2
            cat "$log_path" >&2
            continue
            ;;
    esac

    tap_not_ok_count=$(awk '/^not ok([[:space:]]|$)/ { count += 1 } END { print count + 0 }' "$log_path")
    tap_bail_count=$(awk '/^Bail out!/ { count += 1 } END { print count + 0 }' "$log_path")
    case "${tap_not_ok_count}:${tap_bail_count}" in
        0:0)
            printf 'PASS %s\n' "$label"
            ;;
        *)
            failed=$((failed + 1))
            status=1
            printf 'FAIL %s (tap not-ok=%s bail=%s)\n' \
                "$label" "$tap_not_ok_count" "$tap_bail_count" >&2
            cat "$log_path" >&2
            ;;
    esac
done

case "$run_staticcheck" in
    1|yes|true)
        libwebp_tests=$(printf '%s ' "$src_root"/tests/loader/libwebp/*.t)
        case "${libwebp_tests}" in
            '')
                ;;
            *)
                if ! TEST_FILES="$libwebp_tests" \
                    /bin/sh "$src_root/tests/_static/sh/staticcheck-test-plan-single.sh"; then
                    failed=$((failed + 1))
                    status=1
                    printf 'FAIL staticcheck-test-plan-single (loader/libwebp)\n' >&2
                fi
                if command -v shellcheck >/dev/null 2>&1; then
                    if ! shellcheck "$src_root"/tests/loader/libwebp/*.t; then
                        failed=$((failed + 1))
                        status=1
                        printf 'FAIL shellcheck (loader/libwebp/*.t)\n' >&2
                    fi
                fi
                ;;
        esac
        ;;
    *)
        ;;
esac

if [ "$status" -eq 0 ]; then
    printf 'WebP regression: PASS (%s tests)\n' "$total"
else
    printf 'WebP regression: FAIL (%s/%s failed)\n' "$failed" "$total" >&2
fi

exit "$status"
