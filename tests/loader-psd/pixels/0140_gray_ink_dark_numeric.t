#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify gray ink dark numeric at the public loader boundary.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0266_gray_ink_dark_numeric 1>&2 || {
    echo "not ok 1 - gray ink dark numeric"
    exit 0
}
echo "ok 1 - gray ink dark numeric"
exit 0
