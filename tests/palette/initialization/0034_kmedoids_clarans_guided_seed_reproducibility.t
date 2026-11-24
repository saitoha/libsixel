#!/bin/sh
# TAP test: CLARANS guided mode keeps seeded reproducibility.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-guided-seed-reproducibility \
    >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids CLARANS guided seed reproducibility failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids CLARANS guided mode preserves seed reproducibility"
exit 0
