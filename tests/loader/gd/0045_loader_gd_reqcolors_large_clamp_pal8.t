#!/bin/sh
# TAP wrapper for gd pixelformat case: reqcolors large clamp pal8.

set -eux

test "${HAVE_TEST_RUNNER-}" = 1 || {
    printf "1..0 # SKIP test_runner is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0011_loader_gd_pixelformat" "reqcolors_large_clamp_pal8" || {
    echo "not ok 1 - gd reqcolors_large_clamp_pal8"
    exit 0
}

echo "ok 1 - gd reqcolors_large_clamp_pal8"
exit 0
