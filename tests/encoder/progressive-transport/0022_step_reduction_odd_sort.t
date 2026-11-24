#!/bin/sh
# Verify step reduction odd sort.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0090_step_reduction_odd_sort" || {
    printf '\n%s\n' "not ok 1 - step reduction odd sort"
    exit 0
}
printf '\n%s\n' "ok 1 - step reduction odd sort"
exit 0
