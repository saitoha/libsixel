#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0054_mpet_float_matrix || {
    printf '\n%s\n' "not ok 1 - mpet rectangular matrices preserve signed PCS values and precision"
    exit 0
}

printf '\n%s\n' "ok 1 - mpet rectangular matrices preserve signed PCS values and precision"
exit 0
