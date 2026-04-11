#!/bin/sh
# TAP wrapper for builtin BMP BI_CMYKRLE4 bpp-mismatch rejection checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_FAIL_CMYKRLE4_REQUIRES_4BPP=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail cmykrle4 requires 4bpp numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail cmykrle4 requires 4bpp numeric)"
exit 0
