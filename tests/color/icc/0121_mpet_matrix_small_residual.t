#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0121_mpet_matrix_small_residual || {
    echo "not ok 1 - matrix cancellation retains an amplified residual"
    exit 0
}

echo "ok 1 - matrix cancellation retains an amplified residual"
exit 0
