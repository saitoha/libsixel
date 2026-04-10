#!/bin/sh
# TAP wrapper for gd pixelformat case: rgba background float32.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0011_loader_gd_pixelformat" "rgba_bg_float32" || {
    echo "not ok 1 - gd rgba_bg_float32"
    exit 0
}

echo "ok 1 - gd rgba_bg_float32"
exit 0
