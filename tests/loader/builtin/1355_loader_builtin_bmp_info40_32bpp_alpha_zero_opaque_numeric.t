#!/bin/sh
# TAP wrapper for builtin BMP INFO40 32bpp alpha-zero opaque numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_INFO40_32BPP_ALPHA_ZERO_OPAQUE=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp info40 32bpp alpha zero opaque numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp info40 32bpp alpha zero opaque numeric)"
exit 0
