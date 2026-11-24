#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify identifiers truncated reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0060_identifiers_truncated_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - identifiers truncated reject"
    exit 0
}
printf '\n%s\n' "ok 1 - identifiers truncated reject"
exit 0
