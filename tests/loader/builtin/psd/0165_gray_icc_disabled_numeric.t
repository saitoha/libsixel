#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify gray icc disabled numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0165_gray_icc_disabled_numeric 1>&2 || {
    echo "not ok 1 - gray icc disabled numeric"
    exit 0
}
echo "ok 1 - gray icc disabled numeric"
exit 0
