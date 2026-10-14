#!/bin/sh
# Verify prediction exact joint.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0124_prediction_exact_joint" || {
    echo "not ok 1 - prediction exact joint"
    exit 0
}
echo "ok 1 - prediction exact joint"
exit 0
