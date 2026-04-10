#!/bin/sh
# TAP wrapper for gd pixelformat case: gAMA-only background float32 linear.

set -eux

test "${HAVE_TEST_RUNNER-}" = 1 || {
    printf "1..0 # SKIP test_runner is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0011_loader_gd_pixelformat" "gama_only_bg_float32_linear" || {
    echo "not ok 1 - gd gama_only_bg_float32_linear"
    exit 0
}

echo "ok 1 - gd gama_only_bg_float32_linear"
exit 0
