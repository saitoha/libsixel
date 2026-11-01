#!/bin/sh
# TAP test: k-medoids subset constraint for CLARANS with RGBFLOAT32 samples.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" subset-clarans-float32 >/dev/null || {
    echo "not ok" 1 - "kmedoids subset failed for clarans float32"
    exit 0
}

echo "ok" 1 - "kmedoids subset passed for clarans float32"
exit 0
