#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame clip right atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0054_float_geometry_state bounds 2 || {
    echo "not ok 1 - float geometry clip_right_atomic"
    exit 0
}
echo "ok 1 - float geometry clip_right_atomic"
exit 0
