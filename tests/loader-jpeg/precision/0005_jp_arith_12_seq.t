#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric arith 12 seq samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0071_jp_arith_12_seq" 1>&2 || {
    echo "not ok 1 - Preserve numeric arith 12 seq samples"
    exit 0
}
echo "ok 1 - Preserve numeric arith 12 seq samples"
exit 0
