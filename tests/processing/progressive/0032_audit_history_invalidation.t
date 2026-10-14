#!/bin/sh
# Verify audit history invalidation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0032_audit_history_invalidation" || {
    echo "not ok 1 - audit history invalidation"
    exit 0
}
echo "ok 1 - audit history invalidation"
exit 0
