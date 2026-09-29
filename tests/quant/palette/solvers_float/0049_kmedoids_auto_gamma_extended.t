#!/bin/sh
# Verify kmedoids auto gamma extended.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/solvers_float/0001_solver_contract \
    values 1 0 4 none || {
    echo "not ok 1 - kmedoids auto gamma extended"
    exit 0
}

echo "ok 1 - kmedoids auto gamma extended"
exit 0
