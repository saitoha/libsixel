#!/bin/sh
# TAP test: CLARANS candidate epoch reset matches full clear semantics.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    clarans-candidate-epoch-reset-equivalence \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS candidate epoch reset failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS candidate epoch reset passed"
exit 0
