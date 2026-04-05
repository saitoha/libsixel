#!/bin/sh
# TAP test: CLARANS guided Top-K matches full-sort reference ordering.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-guided-topk-equivalence \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS guided Top-K equivalence failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS guided Top-K equals full-sort reference"
exit 0
