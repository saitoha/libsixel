#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify small contributions independently of large noncontributing samples.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    frame/0098_float_dynamic_range 0 0 0 || {
    echo "not ok 1 - float zero_weight_x_max_frame_scalar"
    exit 0
}
echo "ok 1 - float zero_weight_x_max_frame_scalar"
exit 0
