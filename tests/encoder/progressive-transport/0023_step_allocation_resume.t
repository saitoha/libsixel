#!/bin/sh
# Verify step allocation resume.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0091_step_allocation_resume" || {
    printf '\n%s\n' "not ok 1 - step allocation resume"
    exit 0
}
printf '\n%s\n' "ok 1 - step allocation resume"
exit 0
