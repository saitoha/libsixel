#!/bin/sh
# TAP test: Bandit exact pass cutoff keeps exact best-swap decision.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" bandit-swap-cost-cutoff >/dev/null || {
    echo "not ok" 1 - "kmedoids Bandit swap-cost cutoff consistency failed"
    exit 0
}

echo "ok" 1 - "kmedoids Bandit swap-cost cutoff matches full best swap"
exit 0
