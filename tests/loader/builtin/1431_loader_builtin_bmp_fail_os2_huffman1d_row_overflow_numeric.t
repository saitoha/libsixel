#!/bin/sh
# TAP wrapper for builtin BMP OS/2 HUFFMAN1D row-overflow failure checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_FAIL_OS2_HUFFMAN1D_ROW_OVERFLOW=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail os2 huffman1d row overflow numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail os2 huffman1d row overflow numeric)"
exit 0
