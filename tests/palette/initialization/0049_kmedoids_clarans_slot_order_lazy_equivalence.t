#!/bin/sh
# TAP test: CLARANS lazy slot-order build matches full build.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    clarans-slot-order-lazy-equivalence \
    >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids CLARANS lazy slot-order equivalence failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids CLARANS lazy slot-order equivalence passed"
exit 0
