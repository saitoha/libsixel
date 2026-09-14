#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify float frame resize constant welsh auto.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=auto \
    frame/0018_float_resampling constant 5 || {
    echo "not ok 1 - float resize constant_welsh_auto"
    exit 0
}
echo "ok 1 - float resize constant_welsh_auto"
exit 0
