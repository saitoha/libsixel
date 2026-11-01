#!/bin/sh
# TAP test: CLARANS candidate-centric batch scoring matches pair baseline.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-candidate-batch-equivalence \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS candidate batch equivalence failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS candidate batch equivalence passed"
exit 0
