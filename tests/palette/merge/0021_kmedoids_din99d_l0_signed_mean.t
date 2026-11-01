#!/bin/sh
# Verify kmedoids din99d l0 signed mean.
# Policy: docs/encoder/palette-construction/merge-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/merge_float/0001_merge_contract values 2 4 0 || {
    echo "not ok 1 - kmedoids din99d l0 signed mean"
    exit 0
}

echo "ok 1 - kmedoids din99d l0 signed mean"
exit 0
