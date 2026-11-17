#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0112_mpet_simplex_cancellation_chain || {
    echo "not ok 1 - the next matrix can scale the simplex residual"
    exit 0
}

echo "ok 1 - the next matrix can scale the simplex residual"
exit 0
