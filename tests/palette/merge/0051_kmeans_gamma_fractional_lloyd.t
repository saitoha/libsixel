#!/bin/sh
# Verify K-means gamma fractional population with Lloyd.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract fractional 0 0 3 || {
    echo "not ok 1 - K-means gamma fractional population with Lloyd"
    exit 0
}

echo "ok 1 - K-means gamma fractional population with Lloyd"
exit 0
