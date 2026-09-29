#!/bin/sh
# Verify kmedoids banditpam reduced.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/solvers_float/0001_solver_contract \
    reduced 1 2 3 none || {
    echo "not ok 1 - kmedoids banditpam reduced"
    exit 0
}

echo "ok 1 - kmedoids banditpam reduced"
exit 0
