#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify arith_loss_p4_16_0 allocation failure cleanup.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0142_jp_arith_loss_p4_16_0_allocation" 1>&2 || {
    echo "not ok 1 - arith_loss_p4_16_0 allocation cleanup"
    exit 0
}
echo "ok 1 - arith_loss_p4_16_0 allocation cleanup"
exit 0
