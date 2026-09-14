#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify float frame resize constant bilinear scalar.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    frame/0018_float_resampling constant 4 || {
    echo "not ok 1 - float resize constant_bilinear_scalar"
    exit 0
}
echo "ok 1 - float resize constant_bilinear_scalar"
exit 0
