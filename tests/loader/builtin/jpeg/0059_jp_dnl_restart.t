#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# DNL redefinition preserves restart state.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0059_jp_dnl_restart" 1>&2 || {
    echo "not ok 1 - DNL redefinition preserves restart state"
    exit 0
}
echo "ok 1 - DNL redefinition preserves restart state"
exit 0
