#!/bin/sh
# TAP test: CLARANS swap-cost cutoff keeps exact accept/reject decisions.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-swap-cost-cutoff >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids CLARANS swap-cost cutoff consistency failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids CLARANS swap-cost cutoff matches full decisions"
exit 0
