#!/bin/sh
# TAP test: k-medoids seed reproducibility for CLARANS.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" seed-clarans >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids seed reproducibility failed for clarans"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids seed reproducibility passed for clarans"
exit 0
