#!/bin/sh
# TAP wrapper for gd status case: bad IHDR length returns GD error.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0058_loader_gd_status_policy" \
    "png_bad_ihdr_len_status_gd_error" || {
    echo "not ok 1 - gd png_bad_ihdr_len_status_gd_error"
    exit 0
}

echo "ok 1 - gd png_bad_ihdr_len_status_gd_error"
exit 0
