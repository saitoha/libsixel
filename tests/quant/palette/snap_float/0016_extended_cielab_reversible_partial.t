#!/bin/sh
# Verify native snap: extended cielab reversible partial.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract unit 0 3 2 1 || {
    echo "not ok 1 - extended cielab reversible partial"
    exit 0
}

echo "ok 1 - extended cielab reversible partial"
exit 0
