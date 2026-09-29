#!/bin/sh
# Verify kmedoids zero.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=palette_contract,dither_contract \
    SIXEL_PALETTE_SAMPLE_TARGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/solvers_float/0001_solver_contract \
    zero 1 0 0 1 || {
    echo "not ok 1 - kmedoids zero"
    exit 0
}

echo "ok 1 - kmedoids zero"
exit 0
