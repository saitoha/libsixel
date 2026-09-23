#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric arith rgb loss16 samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0147_jp_arith_rgb_loss16" 1>&2 || {
    echo "not ok 1 - Preserve numeric arith rgb loss16 samples"
    exit 0
}
echo "ok 1 - Preserve numeric arith rgb loss16 samples"
exit 0
