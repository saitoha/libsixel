#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify an exact one-sample Gaussian mean without multiply/divide loss.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    frame/0018_float_resampling identity_large_x 1 || {
    echo "not ok 1 - float identity_large_x_scalar"
    exit 0
}
echo "ok 1 - float identity_large_x_scalar"
exit 0
