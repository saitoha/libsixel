#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0111_mpet_simplex_cancellation || {
    echo "not ok 1 - simplex interpolation retains a residual after cancellation"
    exit 0
}

echo "ok 1 - simplex interpolation retains a residual after cancellation"
exit 0
