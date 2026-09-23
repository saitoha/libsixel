#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# DNL redefinition skips stuffed entropy.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0060_jp_dnl_stuffed" 1>&2 || {
    echo "not ok 1 - DNL redefinition skips stuffed entropy"
    exit 0
}
echo "ok 1 - DNL redefinition skips stuffed entropy"
exit 0
