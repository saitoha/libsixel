#!/bin/sh
# Verify selection dedup keep cost.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0050_selection_dedup_keep_cost" || {
    echo "not ok 1 - selection dedup keep cost"
    exit 0
}
echo "ok 1 - selection dedup keep cost"
exit 0
