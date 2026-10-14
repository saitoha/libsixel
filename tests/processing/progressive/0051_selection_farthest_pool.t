#!/bin/sh
# Verify selection farthest pool.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0051_selection_farthest_pool" || {
    echo "not ok 1 - selection farthest pool"
    exit 0
}
echo "ok 1 - selection farthest pool"
exit 0
