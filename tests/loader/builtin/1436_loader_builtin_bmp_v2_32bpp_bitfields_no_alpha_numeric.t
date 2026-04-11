#!/bin/sh
# TAP wrapper for builtin BMP V2 32bpp BI_BITFIELDS no-alpha checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_V2_32BPP_BITFIELDS_NO_ALPHA=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp v2 32bpp bitfields no alpha numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp v2 32bpp bitfields no alpha numeric)"
exit 0
