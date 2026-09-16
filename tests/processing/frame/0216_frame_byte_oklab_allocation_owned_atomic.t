#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify float frame byte boundary: oklab allocation owned atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0178_float_byte_boundary 2 1 1 1>&2 || {
    echo "not ok 1 - frame byte oklab allocation owned atomic"
    exit 0
}
echo "ok 1 - frame byte oklab allocation owned atomic"
exit 0
