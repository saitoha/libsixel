#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0131_mpet_matrix_product_residual || {
    echo "not ok 1 - matrix product rounding preserves the amplified residual"
    exit 0
}

echo "ok 1 - matrix product rounding preserves the amplified residual"
exit 0
