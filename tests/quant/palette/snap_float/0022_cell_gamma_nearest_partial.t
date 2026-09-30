#!/bin/sh
# Verify native snap: cell gamma nearest partial.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract unit 1 0 1 1 || {
    echo "not ok 1 - cell gamma nearest partial"
    exit 0
}

echo "ok 1 - cell gamma nearest partial"
exit 0
