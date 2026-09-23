#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric arith loss p2 8 0 samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0074_jp_arith_loss_p2_8_0" 1>&2 || {
    echo "not ok 1 - Preserve numeric arith loss p2 8 0 samples"
    exit 0
}
echo "ok 1 - Preserve numeric arith loss p2 8 0 samples"
exit 0
