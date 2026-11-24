#!/bin/sh
# TAP test: CLARANS candidate-centric batch scoring matches pair baseline.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-candidate-batch-equivalence \
    >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids CLARANS candidate batch equivalence failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids CLARANS candidate batch equivalence passed"
exit 0
