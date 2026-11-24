#!/bin/sh
# Verify K-center uses shared binning without changing legacy palette output.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0041_kcenter_binning_filter" || {
    printf '\n%s\n' "not ok 1 - K-center shared-binning regression"
    exit 0
}

printf '\n%s\n' "ok 1 - K-center shared-binning regression"
exit 0
