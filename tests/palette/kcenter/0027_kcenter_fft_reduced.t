#!/bin/sh
# Verify kcenter fft reduced.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/solvers_float/0001_solver_contract \
    reduced 0 2 1 none || {
    printf '\n%s\n' "not ok 1 - kcenter fft reduced"
    exit 0
}

printf '\n%s\n' "ok 1 - kcenter fft reduced"
exit 0
