#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject known-height DNL with excess MCU rows.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0064_jp_dnl_extra_row" 1>&2 || {
    printf '\n%s\n' "not ok 1 - Reject known-height DNL with excess MCU rows"
    exit 0
}
printf '\n%s\n' "ok 1 - Reject known-height DNL with excess MCU rows"
exit 0
