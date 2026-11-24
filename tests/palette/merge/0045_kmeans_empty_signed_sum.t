#!/bin/sh
# Verify kmeans empty cluster signed sum.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract empty 0 0 0 || {
    printf '\n%s\n' "not ok 1 - kmeans empty cluster signed sum"
    exit 0
}

printf '\n%s\n' "ok 1 - kmeans empty cluster signed sum"
exit 0
