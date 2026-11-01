#!/bin/sh
# Verify K-center uses shared binning without changing legacy palette output.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0041_kcenter_binning_filter" || {
    echo "not ok 1 - K-center shared-binning regression"
    exit 0
}

echo "ok 1 - K-center shared-binning regression"
exit 0
