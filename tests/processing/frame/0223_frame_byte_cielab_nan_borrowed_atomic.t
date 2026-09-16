#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify float frame byte boundary: cielab nan borrowed atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0178_float_byte_boundary 3 0 2 1>&2 || {
    echo "not ok 1 - frame byte cielab nan borrowed atomic"
    exit 0
}
echo "ok 1 - frame byte cielab nan borrowed atomic"
exit 0
