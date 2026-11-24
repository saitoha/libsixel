#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify sof3 pred6 against every original sample.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0173_sof3_pred6_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - sof3_pred6 numeric reconstruction"
    exit 0
}
printf '\n%s\n' "ok 1 - sof3_pred6 numeric reconstruction"
exit 0
