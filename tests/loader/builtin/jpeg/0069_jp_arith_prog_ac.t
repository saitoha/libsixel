#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric arith prog ac samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0069_jp_arith_prog_ac" 1>&2 || {
    echo "not ok 1 - Preserve numeric arith prog ac samples"
    exit 0
}
echo "ok 1 - Preserve numeric arith prog ac samples"
exit 0
