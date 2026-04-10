#!/bin/sh
# TAP wrapper for builtin BMP V3 alpha bgcolor float32 numeric checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_V3_32_ALPHA_BGCOLOR_FLOAT32=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp v3 alpha bgcolor float32 numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp v3 alpha bgcolor float32 numeric)"
exit 0
