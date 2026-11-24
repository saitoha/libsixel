#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0121_mpet_matrix_small_residual || {
    printf '\n%s\n' "not ok 1 - matrix cancellation retains an amplified residual"
    exit 0
}

printf '\n%s\n' "ok 1 - matrix cancellation retains an amplified residual"
exit 0
