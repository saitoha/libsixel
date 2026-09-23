#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric hier arith seq samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0090_jp_hier_arith_seq" 1>&2 || {
    echo "not ok 1 - Preserve numeric hier arith seq samples"
    exit 0
}
echo "ok 1 - Preserve numeric hier arith seq samples"
exit 0
