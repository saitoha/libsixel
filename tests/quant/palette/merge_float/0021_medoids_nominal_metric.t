#!/bin/sh
# Verify medoids nominal metric.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract metric 2 0 0 || {
    echo "not ok 1 - medoids nominal metric"
    exit 0
}

echo "ok 1 - medoids nominal metric"
exit 0
