#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify float frame byte boundary: din99d allocation borrowed atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0178_float_byte_boundary 4 0 1 1>&2 || {
    echo "not ok 1 - frame byte din99d allocation borrowed atomic"
    exit 0
}
echo "ok 1 - frame byte din99d allocation borrowed atomic"
exit 0
