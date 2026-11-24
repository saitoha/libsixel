#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# DNL replaces an overestimated SOF height.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0054_jp_dnl_shrink" 1>&2 || {
    printf '\n%s\n' "not ok 1 - DNL replaces an overestimated SOF height"
    exit 0
}
printf '\n%s\n' "ok 1 - DNL replaces an overestimated SOF height"
exit 0
