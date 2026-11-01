#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify frame float overflow atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0015_float_failure_atomic" overflow || {
    echo "not ok 1 - frame float overflow atomic"
    exit 0
}
echo "ok 1 - frame float overflow atomic"
exit 0
