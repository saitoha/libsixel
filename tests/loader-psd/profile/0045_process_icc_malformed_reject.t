#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process icc malformed reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0102_process_icc_malformed_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - process icc malformed reject"
    exit 0
}
printf '\n%s\n' "ok 1 - process icc malformed reject"
exit 0
