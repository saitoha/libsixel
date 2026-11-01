#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric arith rgb float samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0067_jp_arith_rgb_float" 1>&2 || {
    echo "not ok 1 - Preserve numeric arith rgb float samples"
    exit 0
}
echo "ok 1 - Preserve numeric arith rgb float samples"
exit 0
