#!/bin/sh
# TAP wrapper for gd pixelformat case: opaque rgb with bg stays rgb888.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0011_loader_gd_pixelformat" \
    "opaque_rgb_with_bg_stays_rgb888_gamma" || {
    echo "not ok 1 - gd opaque_rgb_with_bg_stays_rgb888_gamma"
    exit 0
}

echo "ok 1 - gd opaque_rgb_with_bg_stays_rgb888_gamma"
exit 0
