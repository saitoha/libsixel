#!/bin/sh
# Verify Ward scan fallback inactive slots after dense-workspace allocation failure.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0002_scan_contract 0 || {
    echo "not ok 1 - Ward scan fallback inactive slots"
    exit 0
}

echo "ok 1 - Ward scan fallback inactive slots"
exit 0
