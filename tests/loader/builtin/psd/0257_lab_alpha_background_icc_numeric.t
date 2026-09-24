#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify lab alpha background icc numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0257_lab_alpha_background_icc_numeric 1>&2 || {
    echo "not ok 1 - lab alpha background icc numeric"
    exit 0
}
echo "ok 1 - lab alpha background icc numeric"
exit 0
