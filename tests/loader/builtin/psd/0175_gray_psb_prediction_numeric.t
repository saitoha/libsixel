#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify gray psb prediction numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0175_gray_psb_prediction_numeric 1>&2 || {
    echo "not ok 1 - gray psb prediction numeric"
    exit 0
}
echo "ok 1 - gray psb prediction numeric"
exit 0
