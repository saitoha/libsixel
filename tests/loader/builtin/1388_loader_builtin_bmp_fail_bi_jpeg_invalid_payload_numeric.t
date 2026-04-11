#!/bin/sh
# TAP wrapper for builtin BMP BI_JPEG invalid-payload fail numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_FAIL_BI_JPEG_INVALID_PAYLOAD=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail bi-jpeg invalid payload numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp fail bi-jpeg invalid payload numeric)"
exit 0
