#!/bin/sh
# Verify audit source refresh.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0030_audit_source_refresh" || {
    printf '\n%s\n' "not ok 1 - audit source refresh"
    exit 0
}
printf '\n%s\n' "ok 1 - audit source refresh"
exit 0
