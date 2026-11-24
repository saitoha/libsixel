#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process spot nan reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0113_process_spot_nan_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - process spot nan reject"
    exit 0
}
printf '\n%s\n' "ok 1 - process spot nan reject"
exit 0
