#!/bin/sh
# Verify step byte budget retry.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0080_step_byte_budget_retry" || {
    printf '\n%s\n' "not ok 1 - step byte budget retry"
    exit 0
}
printf '\n%s\n' "ok 1 - step byte budget retry"
exit 0
