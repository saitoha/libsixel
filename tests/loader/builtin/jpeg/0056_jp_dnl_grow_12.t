#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# DNL growth preserves twelve-bit samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0056_jp_dnl_grow_12" 1>&2 || {
    echo "not ok 1 - DNL growth preserves twelve-bit samples"
    exit 0
}
echo "ok 1 - DNL growth preserves twelve-bit samples"
exit 0
