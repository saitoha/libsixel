#!/bin/sh
# Verify kcenter hard din99d extended.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/solvers_float/0001_solver_contract \
    values 0 4 1 hard || {
    printf '\n%s\n' "not ok 1 - kcenter hard din99d extended"
    exit 0
}

printf '\n%s\n' "ok 1 - kcenter hard din99d extended"
exit 0
