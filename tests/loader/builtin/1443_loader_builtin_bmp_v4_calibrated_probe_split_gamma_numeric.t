#!/bin/sh
# TAP wrapper for builtin BMP V4 calibrated split-gamma probe checks.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BMP_NUMERIC_V4_CALIBRATED_PROBE_SPLIT_GAMMA=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (bmp v4 calibrated probe split gamma numeric)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (bmp v4 calibrated probe split gamma numeric)"
exit 0
