#!/bin/sh
# Verify kcenter auto oklab extended.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/solvers_float/0001_solver_contract \
    values 0 2 0 none || {
    echo "not ok 1 - kcenter auto oklab extended"
    exit 0
}

echo "ok 1 - kcenter auto oklab extended"
exit 0
