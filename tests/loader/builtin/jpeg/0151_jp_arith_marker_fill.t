#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric arith marker fill samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0151_jp_arith_marker_fill" 1>&2 || {
    echo "not ok 1 - Preserve numeric arith marker fill samples"
    exit 0
}
echo "ok 1 - Preserve numeric arith marker fill samples"
exit 0
