#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric arith loss p4 16 3 samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0079_jp_arith_loss_p4_16_3" 1>&2 || {
    echo "not ok 1 - Preserve numeric arith loss p4 16 3 samples"
    exit 0
}
echo "ok 1 - Preserve numeric arith loss p4 16 3 samples"
exit 0
