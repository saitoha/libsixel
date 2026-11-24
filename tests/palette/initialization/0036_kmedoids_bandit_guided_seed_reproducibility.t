#!/bin/sh
# TAP test: Bandit guided candidate generation preserves seed reproducibility.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" bandit-guided-seed-reproducibility \
    >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids bandit guided seed reproducibility failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids bandit guided seed reproducibility passed"
exit 0
