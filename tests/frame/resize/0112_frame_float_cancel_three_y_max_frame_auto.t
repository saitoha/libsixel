#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify small contributions independently of large noncontributing samples.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SIMD_LEVEL=auto \
    frame/0098_float_dynamic_range 5 0 0 || {
    echo "not ok 1 - float cancel_three_y_max_frame_auto"
    exit 0
}
echo "ok 1 - float cancel_three_y_max_frame_auto"
exit 0
