#!/bin/sh
# TAP test: CLARANS slot-order cutoff keeps strict swap decisions unchanged.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-slot-order-cutoff-consistency \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS slot order cutoff consistency failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS slot order cutoff consistency passed"
exit 0
