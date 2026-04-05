#!/bin/sh
# TAP test: Bandit prune row-cache path matches uncached sample costs.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    bandit-prune-row-cache-equivalence \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids bandit prune row-cache equivalence failed"
    exit 0
}

echo "ok" 1 - "kmedoids bandit prune row-cache equivalence passed"
exit 0
