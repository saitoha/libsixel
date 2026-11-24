#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify alternate resource duplicate reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0059_alternate_resource_duplicate_reject 1>&2 || {
    printf '\n%s\n' "not ok 1 - alternate resource duplicate reject"
    exit 0
}
printf '\n%s\n' "ok 1 - alternate resource duplicate reject"
exit 0
