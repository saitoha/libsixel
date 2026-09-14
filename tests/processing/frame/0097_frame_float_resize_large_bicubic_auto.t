#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify finite nonconstant large values against independent normalized weights.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=auto \
    frame/0018_float_resampling large_bicubic 6 || {
    echo "not ok 1 - float resize large_bicubic auto"
    exit 0
}
echo "ok 1 - float resize large_bicubic auto"
exit 0
