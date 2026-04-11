#!/bin/sh
# TAP wrapper for builtin BMP INFO40 pixel-offset-equals-file-size checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_FAIL_I40_PIXEL_OFFSET_EQUALS_FILE_SIZE=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail info40 pixel offset equals file size numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail info40 pixel offset equals file size numeric)"
exit 0
