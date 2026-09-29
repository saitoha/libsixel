#!/bin/sh
# Verify ward signed sums.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract common 0 0 0 || {
    echo "not ok 1 - ward signed sums"
    exit 0
}

echo "ok 1 - ward signed sums"
exit 0
