#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify small contributions independently of large noncontributing samples.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=scalar \
    frame/0098_float_dynamic_range 4 1 0 || {
    echo "not ok 1 - float cancel_seven_1e20_frame_scalar"
    exit 0
}
echo "ok 1 - float cancel_seven_1e20_frame_scalar"
exit 0
