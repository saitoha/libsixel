#!/bin/sh
# TAP test: k-medoids subset constraint for PAM with RGB888 samples.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" subset-pam-8bit >/dev/null || {
    echo "not ok" 1 - "kmedoids subset failed for pam 8bit"
    exit 0
}

echo "ok" 1 - "kmedoids subset passed for pam 8bit"
exit 0
