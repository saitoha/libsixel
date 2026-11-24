#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0132_mpet_simplex_weight_residual || {
    printf '\n%s\n' "not ok 1 - five-axis CLUT retains the amplified weight residual"
    exit 0
}

printf '\n%s\n' "ok 1 - five-axis CLUT retains the amplified weight residual"
exit 0
