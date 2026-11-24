#!/bin/sh
# TAP test: k-medoids subset constraint for CLARA with RGB888 samples.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" subset-clara-8bit >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids subset failed for clara 8bit"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids subset passed for clara 8bit"
exit 0
