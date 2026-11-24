#!/bin/sh
# TAP test: CLARANS active candidate compaction remains consistent.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    clarans-active-candidate-compaction-consistency \
    >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids CLARANS active compaction consistency failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids CLARANS active compaction consistency passed"
exit 0
