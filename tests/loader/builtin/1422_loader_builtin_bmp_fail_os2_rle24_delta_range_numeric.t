#!/bin/sh
# TAP wrapper for builtin BMP OS/2 RLE24 delta-range rejection checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_FAIL_OS2_RLE24_DELTA_RANGE=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail os2 rle24 delta range numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail os2 rle24 delta range numeric)"
exit 0
