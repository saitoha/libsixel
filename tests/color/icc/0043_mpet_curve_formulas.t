#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0043_mpet_curve_formulas || {
    echo "not ok 1 - mpet evaluates the three formula types"
    exit 0
}

echo "ok 1 - mpet evaluates the three formula types"
exit 0
