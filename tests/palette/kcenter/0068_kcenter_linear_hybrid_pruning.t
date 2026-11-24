#!/bin/sh
# Verify linear representatives after hybrid candidate pruning.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/solvers_float/0001_solver_contract \
    pruned 0 1 1 none || {
    printf '\n%s\n' "not ok 1 - linear extended hybrid pruning"
    exit 0
}

printf '\n%s\n' "ok 1 - linear extended hybrid pruning"
exit 0
