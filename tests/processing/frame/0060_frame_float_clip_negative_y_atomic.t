#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame clip negative y atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state bounds 1 || {
    echo "not ok 1 - float geometry clip_negative_y_atomic"
    exit 0
}
echo "ok 1 - float geometry clip_negative_y_atomic"
exit 0
