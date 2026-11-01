#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Preserve numeric hier huff prog samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0085_jp_hier_huff_prog" 1>&2 || {
    echo "not ok 1 - Preserve numeric hier huff prog samples"
    exit 0
}
echo "ok 1 - Preserve numeric hier huff prog samples"
exit 0
