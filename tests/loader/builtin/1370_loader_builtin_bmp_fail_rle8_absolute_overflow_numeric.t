#!/bin/sh
# TAP wrapper for builtin BMP RLE8-absolute-overflow fail numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_FAIL_RLE8_ABSOLUTE_OVERFLOW=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail rle8 absolute overflow numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail rle8 absolute overflow numeric)"
exit 0
