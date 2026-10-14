#!/bin/sh
# Verify step work budget resume.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0081_step_work_budget_resume" || {
    echo "not ok 1 - step work budget resume"
    exit 0
}
echo "ok 1 - step work budget resume"
exit 0
