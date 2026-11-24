#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify display reverse numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0043_display_reverse_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - display reverse numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - display reverse numeric"
exit 0
