#!/bin/sh
# Verify prediction mask boundaries.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0125_prediction_mask_boundaries" || {
    echo "not ok 1 - prediction mask boundaries"
    exit 0
}
echo "ok 1 - prediction mask boundaries"
exit 0
