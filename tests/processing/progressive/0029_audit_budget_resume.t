#!/bin/sh
# Verify audit budget resume.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0029_audit_budget_resume" || {
    echo "not ok 1 - audit budget resume"
    exit 0
}
echo "ok 1 - audit budget resume"
exit 0
