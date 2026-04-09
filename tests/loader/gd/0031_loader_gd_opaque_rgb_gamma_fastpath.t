#!/bin/sh
# TAP wrapper for gd pixelformat case: opaque rgb gamma fast path.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMJPEGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMJPEGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0011_loader_gd_pixelformat" "opaque_rgb_gamma_fastpath" || {
    echo "not ok 1 - gd opaque_rgb_gamma_fastpath"
    exit 0
}

echo "ok 1 - gd opaque_rgb_gamma_fastpath"
exit 0
