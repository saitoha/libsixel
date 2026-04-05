#!/bin/sh
# TAP test: CLARANS row-generation cache keeps swap-cost equivalence.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    clarans-row-cache-generation-equivalence \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS row cache generation failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS row cache generation passed"
exit 0
