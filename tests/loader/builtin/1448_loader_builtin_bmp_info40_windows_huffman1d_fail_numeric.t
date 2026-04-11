#!/bin/sh
# TAP wrapper for builtin BMP info40 windows-mode HUFFMAN1D fail checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_INFO40_WINDOWS_HUFFMAN1D_FAIL=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp info40 windows huffman1d fail)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp info40 windows huffman1d fail)"
exit 0
