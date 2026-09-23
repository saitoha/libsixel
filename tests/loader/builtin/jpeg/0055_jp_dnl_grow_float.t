#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# DNL growth preserves explicit float output.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0055_jp_dnl_grow_float" 1>&2 || {
    echo "not ok 1 - DNL growth preserves explicit float output"
    exit 0
}
echo "ok 1 - DNL growth preserves explicit float output"
exit 0
