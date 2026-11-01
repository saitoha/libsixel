#!/bin/sh
# TAP test: Bandit prune unique sample extraction is seed reproducible.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    bandit-prune-unique-sample-seed-reproducibility \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids bandit unique sample seed failed"
    exit 0
}

echo "ok" 1 - "kmedoids bandit unique sample seed passed"
exit 0
