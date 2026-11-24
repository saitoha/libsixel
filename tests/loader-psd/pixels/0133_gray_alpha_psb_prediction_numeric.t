#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify gray alpha psb prediction numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0209_gray_alpha_psb_prediction_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - gray alpha psb prediction numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - gray alpha psb prediction numeric"
exit 0
