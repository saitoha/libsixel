#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0103_mpet_simplex_zero_weight || {
    echo "not ok 1 - high dimensional simplex ignores extreme zero-weight samples"
    exit 0
}

echo "ok 1 - high dimensional simplex ignores extreme zero-weight samples"
exit 0
