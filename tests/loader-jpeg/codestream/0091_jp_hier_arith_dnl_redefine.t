#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric hier arith dnl redefine samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0155_jp_hier_arith_dnl_redefine" 1>&2 || {
    echo "not ok 1 - Preserve numeric hier arith dnl redefine samples"
    exit 0
}
echo "ok 1 - Preserve numeric hier arith dnl redefine samples"
exit 0
