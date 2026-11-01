#!/bin/sh
# Verify kcenter invalid.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/solvers_float/0001_solver_contract \
    invalid 0 0 0 none || {
    echo "not ok 1 - kcenter invalid"
    exit 0
}

echo "ok 1 - kcenter invalid"
exit 0
