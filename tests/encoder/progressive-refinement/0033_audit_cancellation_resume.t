#!/bin/sh
# Verify audit cancellation resume.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0033_audit_cancellation_resume" || {
    echo "not ok 1 - audit cancellation resume"
    exit 0
}
echo "ok 1 - audit cancellation resume"
exit 0
