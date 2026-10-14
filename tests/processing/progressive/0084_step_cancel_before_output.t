#!/bin/sh
# Verify step cancel before output.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0084_step_cancel_before_output" || {
    echo "not ok 1 - step cancel before output"
    exit 0
}
echo "ok 1 - step cancel before output"
exit 0
