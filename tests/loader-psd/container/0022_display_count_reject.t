#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify display count reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0032_display_count_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - display count reject"
    exit 0
}
printf '\n%s\n' "ok 1 - display count reject"
exit 0
