#!/bin/sh
# Verify kmeans gamma l3 signed mean.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract values 0 0 3 || {
    printf '\n%s\n' "not ok 1 - kmeans gamma l3 signed mean"
    exit 0
}

printf '\n%s\n' "ok 1 - kmeans gamma l3 signed mean"
exit 0
