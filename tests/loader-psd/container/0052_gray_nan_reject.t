#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify gray nan reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0171_gray_nan_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - gray nan reject"
    exit 0
}
printf '\n%s\n' "ok 1 - gray nan reject"
exit 0
