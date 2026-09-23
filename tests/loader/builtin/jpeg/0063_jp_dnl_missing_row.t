#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject known-height DNL with missing MCU rows.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0063_jp_dnl_missing_row" 1>&2 || {
    echo "not ok 1 - Reject known-height DNL with missing MCU rows"
    exit 0
}
echo "ok 1 - Reject known-height DNL with missing MCU rows"
exit 0
