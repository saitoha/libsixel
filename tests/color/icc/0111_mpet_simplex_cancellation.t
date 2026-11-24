#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0111_mpet_simplex_cancellation || {
    printf '\n%s\n' "not ok 1 - simplex interpolation retains a residual after cancellation"
    exit 0
}

printf '\n%s\n' "ok 1 - simplex interpolation retains a residual after cancellation"
exit 0
