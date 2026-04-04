#!/bin/sh
# TAP test: Bandit delta assignment update stays consistent with full assign.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" bandit-delta-consistency >/dev/null || {
    echo "not ok" 1 - "kmedoids Bandit delta consistency check failed"
    exit 0
}

echo "ok" 1 - "kmedoids Bandit delta update matches full assignment"
exit 0
