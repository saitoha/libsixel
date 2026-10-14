#!/bin/sh
# Verify audit revision exhaustion.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0034_audit_revision_exhaustion" || {
    echo "not ok 1 - audit revision exhaustion"
    exit 0
}
echo "ok 1 - audit revision exhaustion"
exit 0
