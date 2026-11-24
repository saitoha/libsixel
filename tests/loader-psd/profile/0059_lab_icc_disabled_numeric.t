#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify lab icc disabled numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0181_lab_icc_disabled_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - lab icc disabled numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - lab icc disabled numeric"
exit 0
