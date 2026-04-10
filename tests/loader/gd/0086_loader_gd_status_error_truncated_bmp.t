#!/bin/sh
# TAP wrapper for gd status error_truncated_bmp.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0058_loader_gd_status_policy" "error_truncated_bmp" || {
    echo "not ok 1 - gd status error_truncated_bmp"
    exit 0
}

echo "ok 1 - gd status error_truncated_bmp"
exit 0
