#!/bin/sh
# TAP test: CLARANS delta refresh fallback matches full refresh behavior.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-delta-refresh-fallback \
    >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids CLARANS delta refresh fallback consistency failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids CLARANS delta refresh fallback consistency passed"
exit 0
