#!/bin/sh
# TAP wrapper for builtin BMP INFO40 2bpp palette overflow checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_FAIL_I40_2BPP_PALETTE_OVERFLOW=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp info40 2bpp palette overflow)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp info40 2bpp palette overflow)"
exit 0
