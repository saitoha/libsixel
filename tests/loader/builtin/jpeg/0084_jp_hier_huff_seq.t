#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric hier huff seq samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0084_jp_hier_huff_seq" 1>&2 || {
    echo "not ok 1 - Preserve numeric hier huff seq samples"
    exit 0
}
echo "ok 1 - Preserve numeric hier huff seq samples"
exit 0
