#!/bin/sh
# Verify P3 destination grid: full cielab.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/p3_snap/0001_grid 4 3 || {
    echo "not ok 1 - P3 grid full cielab"
    exit 0
}

echo "ok 1 - P3 grid full cielab"
exit 0
