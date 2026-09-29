#!/bin/sh
# Verify gamma representatives after hybrid candidate pruning.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/solvers_float/0001_solver_contract \
    pruned 0 0 1 none || {
    echo "not ok 1 - gamma extended hybrid pruning"
    exit 0
}

echo "ok 1 - gamma extended hybrid pruning"
exit 0
