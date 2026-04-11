#!/bin/sh
# TAP wrapper for builtin BMP info40 auto-mode RLE24 numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_INFO40_AUTO_RLE24=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp info40 auto rle24 numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp info40 auto rle24 numeric)"
exit 0
