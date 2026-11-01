#!/bin/sh
# TAP test: CLARANS adaptive slot-probe target is deterministic.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    clarans-adaptive-slot-probe-budget-consistency-v12 \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS adaptive slot probe v12 failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS adaptive slot probe v12 passed"
exit 0
