#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify lab8 icc numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0179_lab8_icc_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - lab8 icc numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - lab8 icc numeric"
exit 0
