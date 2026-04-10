#!/bin/sh
# TAP wrapper for gd can_try interlaced_png_delegate_false.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0057_loader_gd_can_try_policy" "interlaced_png_delegate_false" || {
    echo "not ok 1 - gd can_try interlaced_png_delegate_false"
    exit 0
}

echo "ok 1 - gd can_try interlaced_png_delegate_false"
exit 0
