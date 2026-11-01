#!/bin/sh
# Verify step reduction odd sort.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0090_step_reduction_odd_sort" || {
    echo "not ok 1 - step reduction odd sort"
    exit 0
}
echo "ok 1 - step reduction odd sort"
exit 0
