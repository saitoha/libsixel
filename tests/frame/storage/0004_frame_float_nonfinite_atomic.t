#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify frame float nonfinite atomic.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0015_float_failure_atomic" nonfinite || {
    printf '\n%s\n' "not ok 1 - frame float nonfinite atomic"
    exit 0
}
printf '\n%s\n' "ok 1 - frame float nonfinite atomic"
exit 0
