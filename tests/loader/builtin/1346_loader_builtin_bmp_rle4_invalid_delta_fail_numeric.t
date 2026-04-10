#!/bin/sh
# TAP wrapper for builtin BMP RLE4 invalid-delta fail numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_RLE4_INVALID_DELTA_FAIL=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp rle4 invalid delta fail numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp rle4 invalid delta fail numeric)"
exit 0
