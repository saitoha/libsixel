#!/bin/sh
# TAP wrapper for gd pixelformat case: indexed keycolor gama/iccp bg float32.

set -eux

test "${HAVE_DECL_GDIMAGECREATEFROMPNGPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMPNGPTR is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0011_loader_gd_pixelformat" \
    "indexed_keycolor_gama_iccp_bg_float32_linear" || {
    echo "not ok 1 - gd indexed_keycolor_gama_iccp_bg_float32_linear"
    exit 0
}

echo "ok 1 - gd indexed_keycolor_gama_iccp_bg_float32_linear"
exit 0
