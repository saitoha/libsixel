#!/bin/sh
# Verify step cancel after admission.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0085_step_cancel_after_admission" || {
    printf '\n%s\n' "not ok 1 - step cancel after admission"
    exit 0
}
printf '\n%s\n' "ok 1 - step cancel after admission"
exit 0
