#!/bin/sh
# Verify audit threshold.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0025_audit_threshold" || {
    echo "not ok 1 - audit threshold"
    exit 0
}
echo "ok 1 - audit threshold"
exit 0
