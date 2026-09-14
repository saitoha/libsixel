#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify float frame byte boundary: dimension overflow owned atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0178_float_byte_boundary 1 1 6 1>&2 || {
    echo "not ok 1 - frame byte dimension overflow owned atomic"
    exit 0
}
echo "ok 1 - frame byte dimension overflow owned atomic"
exit 0
