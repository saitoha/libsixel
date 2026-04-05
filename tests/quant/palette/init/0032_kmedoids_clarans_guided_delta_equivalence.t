#!/bin/sh
# TAP test: CLARANS guided delta update matches full rebuild sets.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-guided-delta-equivalence \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS guided delta equivalence failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS guided delta matches full rebuild"
exit 0
