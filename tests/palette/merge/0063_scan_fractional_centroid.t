#!/bin/sh
# Verify Ward scan fallback fractional centroid after dense-workspace allocation failure.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0002_scan_contract 1 || {
    echo "not ok 1 - Ward scan fallback fractional centroid"
    exit 0
}

echo "ok 1 - Ward scan fallback fractional centroid"
exit 0
