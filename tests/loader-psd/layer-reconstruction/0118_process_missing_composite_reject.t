#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify process missing composite reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0121_process_missing_composite_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - process missing composite reject"
    exit 0
}
printf '\n%s\n' "ok 1 - process missing composite reject"
exit 0
