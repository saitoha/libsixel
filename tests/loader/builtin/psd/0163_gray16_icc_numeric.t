#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify gray16 icc numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0163_gray16_icc_numeric 1>&2 || {
    echo "not ok 1 - gray16 icc numeric"
    exit 0
}
echo "ok 1 - gray16 icc numeric"
exit 0
