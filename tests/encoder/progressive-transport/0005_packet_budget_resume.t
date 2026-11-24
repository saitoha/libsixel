#!/bin/sh
# Verify packet budget resume.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0073_packet_budget_resume" || {
    printf '\n%s\n' "not ok 1 - packet budget resume"
    exit 0
}
printf '\n%s\n' "ok 1 - packet budget resume"
exit 0
