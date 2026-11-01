#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric arith loss p4 2 0 samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0080_jp_arith_loss_p4_2_0" 1>&2 || {
    echo "not ok 1 - Preserve numeric arith loss p4 2 0 samples"
    exit 0
}
echo "ok 1 - Preserve numeric arith loss p4 2 0 samples"
exit 0
