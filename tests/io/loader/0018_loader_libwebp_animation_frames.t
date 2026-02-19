#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_test_runner "loader/0018_loader_libwebp_animation_frames" || {
    echo "not ok 1 - loader/0018_loader_libwebp_animation_frames"
    exit 0
}

echo "ok 1 - loader/0018_loader_libwebp_animation_frames"
exit 0
