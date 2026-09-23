#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric arith prog dnl zero samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0149_jp_arith_prog_dnl_zero" 1>&2 || {
    echo "not ok 1 - Preserve numeric arith prog dnl zero samples"
    exit 0
}
echo "ok 1 - Preserve numeric arith prog dnl zero samples"
exit 0
