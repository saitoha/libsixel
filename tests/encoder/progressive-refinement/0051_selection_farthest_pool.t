#!/bin/sh
# Verify selection farthest pool.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0051_selection_farthest_pool" || {
    printf '\n%s\n' "not ok 1 - selection farthest pool"
    exit 0
}
printf '\n%s\n' "ok 1 - selection farthest pool"
exit 0
