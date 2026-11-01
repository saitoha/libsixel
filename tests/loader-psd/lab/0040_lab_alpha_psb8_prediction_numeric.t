#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify lab alpha psb8 prediction numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0259_lab_alpha_psb8_prediction_numeric 1>&2 || {
    echo "not ok 1 - lab alpha psb8 prediction numeric"
    exit 0
}
echo "ok 1 - lab alpha psb8 prediction numeric"
exit 0
