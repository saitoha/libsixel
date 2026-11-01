#!/bin/sh
# TAP test: k-medoids seed reproducibility for BanditPAM.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" seed-banditpam >/dev/null || {
    echo "not ok" 1 - "kmedoids seed reproducibility failed for banditpam"
    exit 0
}

echo "ok" 1 - "kmedoids seed reproducibility passed for banditpam"
exit 0
