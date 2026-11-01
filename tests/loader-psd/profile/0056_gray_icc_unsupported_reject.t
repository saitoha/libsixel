#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify gray icc unsupported reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0170_gray_icc_unsupported_reject 1>&2 || {
    echo "not ok 1 - gray icc unsupported reject"
    exit 0
}
echo "ok 1 - gray icc unsupported reject"
exit 0
