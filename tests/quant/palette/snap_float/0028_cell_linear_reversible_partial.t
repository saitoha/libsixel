#!/bin/sh
# Verify native snap: cell linear reversible partial.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract unit 1 1 2 1 || {
    echo "not ok 1 - cell linear reversible partial"
    exit 0
}

echo "ok 1 - cell linear reversible partial"
exit 0
