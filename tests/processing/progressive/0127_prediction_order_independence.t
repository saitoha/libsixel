#!/bin/sh
# Verify prediction order independence.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0127_prediction_order_independence" || {
    echo "not ok 1 - prediction order independence"
    exit 0
}
echo "ok 1 - prediction order independence"
exit 0
