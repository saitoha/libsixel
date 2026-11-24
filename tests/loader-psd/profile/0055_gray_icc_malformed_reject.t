#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify gray icc malformed reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0169_gray_icc_malformed_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - gray icc malformed reject"
    exit 0
}
printf '\n%s\n' "ok 1 - gray icc malformed reject"
exit 0
