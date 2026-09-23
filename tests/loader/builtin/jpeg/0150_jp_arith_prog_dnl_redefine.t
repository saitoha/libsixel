#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric arith prog dnl redefine samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0150_jp_arith_prog_dnl_redefine" 1>&2 || {
    echo "not ok 1 - Preserve numeric arith prog dnl redefine samples"
    exit 0
}
echo "ok 1 - Preserve numeric arith prog dnl redefine samples"
exit 0
