#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0128_mpet_curve_small_residual || {
    printf '\n%s\n' "not ok 1 - sampled curve retains an amplified residual"
    exit 0
}

printf '\n%s\n' "ok 1 - sampled curve retains an amplified residual"
exit 0
