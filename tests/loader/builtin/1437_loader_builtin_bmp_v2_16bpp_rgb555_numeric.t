#!/bin/sh
# TAP wrapper for builtin BMP V2 16bpp BI_RGB default-mask checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_V2_16BPP_RGB555=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp v2 16bpp rgb555 numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp v2 16bpp rgb555 numeric)"
exit 0
