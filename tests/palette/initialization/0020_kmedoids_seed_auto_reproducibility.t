#!/bin/sh
# TAP test: k-medoids seed reproducibility for auto solver selection.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" seed-auto >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids seed reproducibility failed for auto"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids seed reproducibility passed for auto"
exit 0
