#!/bin/sh
# Verify audit history invalidation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0032_audit_history_invalidation" || {
    printf '\n%s\n' "not ok 1 - audit history invalidation"
    exit 0
}
printf '\n%s\n' "ok 1 - audit history invalidation"
exit 0
