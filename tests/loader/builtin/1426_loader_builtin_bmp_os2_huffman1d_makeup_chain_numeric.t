#!/bin/sh
# TAP wrapper for builtin BMP OS/2 HUFFMAN1D makeup-chain decode checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_OS2_HUFFMAN1D_MAKEUP_CHAIN=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp os2 huffman1d makeup chain numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp os2 huffman1d makeup chain numeric)"
exit 0
