#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify alternate truncated reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0058_alternate_truncated_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - alternate truncated reject"
    exit 0
}
printf '\n%s\n' "ok 1 - alternate truncated reject"
exit 0
