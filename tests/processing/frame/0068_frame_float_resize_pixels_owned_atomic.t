#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify float frame resize pixels owned atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state resize 1 owned || {
    echo "not ok 1 - float geometry resize_pixels_owned_atomic"
    exit 0
}
echo "ok 1 - float geometry resize_pixels_owned_atomic"
exit 0
