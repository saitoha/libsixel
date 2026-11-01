#!/bin/sh
# TAP test: Bandit prune partial-select matches full-sort reference.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    bandit-prune-partial-select-equivalence \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids bandit partial select equivalence failed"
    exit 0
}

echo "ok" 1 - "kmedoids bandit partial select equivalence passed"
exit 0
