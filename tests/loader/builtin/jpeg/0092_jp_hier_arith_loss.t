#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric hier arith loss samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0092_jp_hier_arith_loss" 1>&2 || {
    echo "not ok 1 - Preserve numeric hier arith loss samples"
    exit 0
}
echo "ok 1 - Preserve numeric hier arith loss samples"
exit 0
