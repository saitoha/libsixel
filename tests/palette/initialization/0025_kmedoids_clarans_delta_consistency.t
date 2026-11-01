#!/bin/sh
# TAP test: CLARANS delta assignment update stays consistent with full assign.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-delta-consistency >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS delta consistency check failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS delta update matches full assignment"
exit 0
