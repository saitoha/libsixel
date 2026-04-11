#!/bin/sh
# TAP wrapper for builtin BMP OS/2 short DIB(16) RGB24 numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_OS2_SHORT16_RGB24=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp os2 short16 rgb24 numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp os2 short16 rgb24 numeric)"
exit 0
