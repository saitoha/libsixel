#!/bin/sh
# TAP wrapper for builtin BMP BI_CMYK requires-32bpp fail checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_FAIL_CMYK_REQUIRES_32BPP=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail cmyk requires 32bpp numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail cmyk requires 32bpp numeric)"
exit 0
