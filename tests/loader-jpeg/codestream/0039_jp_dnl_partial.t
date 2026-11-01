#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# DNL adjusts height within the final MCU row.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0062_jp_dnl_partial" 1>&2 || {
    echo "not ok 1 - DNL adjusts height within the final MCU row"
    exit 0
}
echo "ok 1 - DNL adjusts height within the final MCU row"
exit 0
