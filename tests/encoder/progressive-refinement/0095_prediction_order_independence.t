#!/bin/sh
# Verify prediction order independence.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0127_prediction_order_independence" || {
    printf '\n%s\n' "not ok 1 - prediction order independence"
    exit 0
}
printf '\n%s\n' "ok 1 - prediction order independence"
exit 0
