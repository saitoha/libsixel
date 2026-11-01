#!/bin/sh
# Verify step byte budget retry.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0080_step_byte_budget_retry" || {
    echo "not ok 1 - step byte budget retry"
    exit 0
}
echo "ok 1 - step byte budget retry"
exit 0
