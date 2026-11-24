#!/bin/sh
# Verify audit threshold.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0025_audit_threshold" || {
    printf '\n%s\n' "not ok 1 - audit threshold"
    exit 0
}
printf '\n%s\n' "ok 1 - audit threshold"
exit 0
