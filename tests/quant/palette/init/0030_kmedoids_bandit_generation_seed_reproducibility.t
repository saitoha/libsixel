#!/bin/sh
# TAP test: Bandit generation hash keeps seeded reproducibility.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" bandit-generation-seed >/dev/null || {
    echo "not ok" 1 - "kmedoids Bandit generation seed reproducibility failed"
    exit 0
}

echo "ok" 1 - "kmedoids Bandit generation hash preserves seed reproducibility"
exit 0
