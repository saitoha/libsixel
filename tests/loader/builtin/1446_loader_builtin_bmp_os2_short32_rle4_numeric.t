#!/bin/sh
# TAP wrapper for builtin BMP OS/2 short DIB(32) RLE4 numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_OS2_SHORT32_RLE4=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp os2 short32 rle4 numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp os2 short32 rle4 numeric)"
exit 0
