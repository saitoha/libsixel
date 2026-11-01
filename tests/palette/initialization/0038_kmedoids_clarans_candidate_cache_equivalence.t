#!/bin/sh
# TAP test: CLARANS candidate distance cache keeps swap decisions unchanged.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-candidate-cache-equivalence \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS candidate cache equivalence failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS candidate cache equivalence passed"
exit 0
