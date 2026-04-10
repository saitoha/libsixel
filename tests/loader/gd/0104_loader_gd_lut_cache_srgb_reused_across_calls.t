#!/bin/sh
# TAP wrapper for gd status case: sRGB LUT cache reused across calls.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0058_loader_gd_status_policy" \
    "lut_cache_srgb_reused_across_calls" || {
    echo "not ok 1 - gd lut_cache_srgb_reused_across_calls"
    exit 0
}

echo "ok 1 - gd lut_cache_srgb_reused_across_calls"
exit 0
