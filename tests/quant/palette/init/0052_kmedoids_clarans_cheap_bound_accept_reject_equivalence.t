#!/bin/sh
# TAP test: CLARANS cheap bound keeps strict accept/reject equivalence.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    clarans-cheap-bound-accept-reject-equivalence \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS cheap bound equivalence failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS cheap bound equivalence passed"
exit 0
