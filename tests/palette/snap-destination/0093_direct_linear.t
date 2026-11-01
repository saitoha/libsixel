#!/bin/sh
# Verify P3 destination grid: direct linear.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/p3_snap/0001_grid 1 1 || {
    echo "not ok 1 - P3 grid direct linear"
    exit 0
}

echo "ok 1 - P3 grid direct linear"
exit 0
