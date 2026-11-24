#!/bin/sh
# TAP test: CLARANS dirty slot-order rebuild matches full rebuild.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    clarans-slot-order-dirty-rebuild-consistency \
    >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids CLARANS dirty slot-order rebuild failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids CLARANS dirty slot-order rebuild passed"
exit 0
