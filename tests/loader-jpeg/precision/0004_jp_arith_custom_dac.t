#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric arith custom dac samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0070_jp_arith_custom_dac" 1>&2 || {
    echo "not ok 1 - Preserve numeric arith custom dac samples"
    exit 0
}
echo "ok 1 - Preserve numeric arith custom dac samples"
exit 0
