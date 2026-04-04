#!/bin/sh
# TAP test: k-medoids stochastic algorithms are deterministic with fixed seed.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" --seed >/dev/null || {
    echo "not ok" 1 - "kmedoids seed reproducibility check failed"
    exit 0
}

echo "ok" 1 - "kmedoids seed reproducibility check passed"
exit 0
