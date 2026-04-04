#!/bin/sh
# TAP test: k-medoids seed reproducibility for CLARA.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" seed-clara >/dev/null || {
    echo "not ok" 1 - "kmedoids seed reproducibility failed for clara"
    exit 0
}

echo "ok" 1 - "kmedoids seed reproducibility passed for clara"
exit 0
