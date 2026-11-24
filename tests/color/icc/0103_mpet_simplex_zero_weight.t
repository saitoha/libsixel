#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0103_mpet_simplex_zero_weight || {
    printf '\n%s\n' "not ok 1 - high dimensional simplex ignores extreme zero-weight samples"
    exit 0
}

printf '\n%s\n' "ok 1 - high dimensional simplex ignores extreme zero-weight samples"
exit 0
