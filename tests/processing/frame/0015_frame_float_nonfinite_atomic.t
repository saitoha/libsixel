#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify frame float nonfinite atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0015_float_failure_atomic" nonfinite || {
    echo "not ok 1 - frame float nonfinite atomic"
    exit 0
}
echo "ok 1 - frame float nonfinite atomic"
exit 0
