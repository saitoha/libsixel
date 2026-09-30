#!/bin/sh
# Verify kmeans din99d fractional Ward population through executed soft binning.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract fractional 0 4 0 || {
    echo "not ok 1 - kmeans din99d fractional Ward population"
    exit 0
}

echo "ok 1 - kmeans din99d fractional Ward population"
exit 0
