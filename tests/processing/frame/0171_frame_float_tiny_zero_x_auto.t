#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify exact subnormal or zero samples through a two-contributor mean.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=auto \
    frame/0018_float_resampling tiny_zero_x 4 || {
    echo "not ok 1 - float tiny_zero_x_auto"
    exit 0
}
echo "ok 1 - float tiny_zero_x_auto"
exit 0
