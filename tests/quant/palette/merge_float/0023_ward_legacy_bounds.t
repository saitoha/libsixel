#!/bin/sh
# Verify ward legacy bounds.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract common 0 0 1 || {
    echo "not ok 1 - ward legacy bounds"
    exit 0
}

echo "ok 1 - ward legacy bounds"
exit 0
