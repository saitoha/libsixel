#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric arith gray byte samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0066_jp_arith_gray_byte" 1>&2 || {
    echo "not ok 1 - Preserve numeric arith gray byte samples"
    exit 0
}
echo "ok 1 - Preserve numeric arith gray byte samples"
exit 0
