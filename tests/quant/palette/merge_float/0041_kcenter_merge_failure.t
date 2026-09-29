#!/bin/sh
# Verify kcenter merge failure and recovery.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract failure 1 0 0 || {
    echo "not ok 1 - kcenter merge failure and recovery"
    exit 0
}

echo "ok 1 - kcenter merge failure and recovery"
exit 0
