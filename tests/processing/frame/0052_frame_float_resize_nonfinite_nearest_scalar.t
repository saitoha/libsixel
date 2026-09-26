#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame resize nonfinite nearest scalar.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    frame/0018_float_resampling nonfinite 0 || {
    echo "not ok 1 - float resize nonfinite_nearest_scalar"
    exit 0
}
echo "ok 1 - float resize nonfinite_nearest_scalar"
exit 0
