#!/bin/sh
# TAP wrapper for builtin BMP INFO40 rgb32 image-size overflow checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_FAIL_I40_RGB32_IMAGE_SIZE_OVERFLOW=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail info40 rgb32 image size overflow numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail info40 rgb32 image size overflow numeric)"
exit 0
