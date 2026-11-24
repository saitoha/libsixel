#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify sof3 pred3 against every original sample.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0170_sof3_pred3_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - sof3_pred3 numeric reconstruction"
    exit 0
}
printf '\n%s\n' "ok 1 - sof3_pred3 numeric reconstruction"
exit 0
