#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame resize constant gaussian auto.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=auto \
    frame/0018_float_resampling constant 1 || {
    echo "not ok 1 - float resize constant_gaussian_auto"
    exit 0
}
echo "ok 1 - float resize constant_gaussian_auto"
exit 0
