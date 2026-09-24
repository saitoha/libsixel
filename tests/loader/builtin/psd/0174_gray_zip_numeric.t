#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify gray zip numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0174_gray_zip_numeric 1>&2 || {
    echo "not ok 1 - gray zip numeric"
    exit 0
}
echo "ok 1 - gray zip numeric"
exit 0
