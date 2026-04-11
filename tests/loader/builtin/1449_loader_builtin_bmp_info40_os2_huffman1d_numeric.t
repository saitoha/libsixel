#!/bin/sh
# TAP wrapper for builtin BMP info40 os2-mode HUFFMAN1D numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_INFO40_OS2_HUFFMAN1D=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp info40 os2 huffman1d numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp info40 os2 huffman1d numeric)"
exit 0
