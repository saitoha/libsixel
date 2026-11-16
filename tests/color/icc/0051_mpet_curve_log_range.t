#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0051_mpet_curve_log_range || {
    echo "not ok 1 - mpet logarithms survive intermediate power overflow and underflow"
    exit 0
}

echo "ok 1 - mpet logarithms survive intermediate power overflow and underflow"
exit 0
