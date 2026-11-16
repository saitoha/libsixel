#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0081_mpet_clut_budget_recovery || {
    echo "not ok 1 - mpet float CLUT storage shares the allocation limit and permits retry"
    exit 0
}

echo "ok 1 - mpet float CLUT storage shares the allocation limit and permits retry"
exit 0
