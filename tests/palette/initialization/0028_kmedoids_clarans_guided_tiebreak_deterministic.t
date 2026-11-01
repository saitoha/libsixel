#!/bin/sh
# TAP test: CLARANS guided lists keep deterministic tie-break order.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-guided-tiebreak >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS guided tie-break check failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS guided tie-break ordering is deterministic"
exit 0
