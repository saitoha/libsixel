#!/bin/sh
# TAP wrapper for gd pixelformat case: highdepth sRGB-only float32 linear.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0011_loader_gd_pixelformat" \
    "highdepth_srgb_only_float32_linear" || {
    echo "not ok 1 - gd highdepth_srgb_only_float32_linear"
    exit 0
}

echo "ok 1 - gd highdepth_srgb_only_float32_linear"
exit 0
