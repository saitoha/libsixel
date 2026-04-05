#!/bin/sh
# TAP test: CLARANS non-medoid pool index map stays bidirectionally valid.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    clarans-nonmedoid-pool-indexmap-consistency \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS non-medoid pool map failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS non-medoid pool map passed"
exit 0
