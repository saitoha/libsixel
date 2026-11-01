#!/bin/sh
# Verify prediction population weights.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0126_prediction_population_weights" || {
    echo "not ok 1 - prediction population weights"
    exit 0
}
echo "ok 1 - prediction population weights"
exit 0
