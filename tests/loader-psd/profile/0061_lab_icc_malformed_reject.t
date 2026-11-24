#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify lab icc malformed reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0185_lab_icc_malformed_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - lab icc malformed reject"
    exit 0
}
printf '\n%s\n' "ok 1 - lab icc malformed reject"
exit 0
