#!/bin/sh
# Verify native snap: cell oklab reversible full.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract unit 1 2 2 0 || {
    echo "not ok 1 - cell oklab reversible full"
    exit 0
}

echo "ok 1 - cell oklab reversible full"
exit 0
