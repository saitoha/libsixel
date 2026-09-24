#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify lab alpha psb prediction numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0247_lab_alpha_psb_prediction_numeric 1>&2 || {
    echo "not ok 1 - lab alpha psb prediction numeric"
    exit 0
}
echo "ok 1 - lab alpha psb prediction numeric"
exit 0
