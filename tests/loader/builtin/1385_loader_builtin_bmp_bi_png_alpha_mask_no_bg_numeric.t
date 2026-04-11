#!/bin/sh
# TAP wrapper for builtin BMP BI_PNG alpha-mask no-bg numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_BI_PNG_ALPHA_MASK_NO_BG=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp bi-png alpha mask no-bg numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp bi-png alpha mask no-bg numeric)"
exit 0
