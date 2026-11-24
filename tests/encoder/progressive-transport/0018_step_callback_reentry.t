#!/bin/sh
# Verify step callback reentry.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0086_step_callback_reentry" || {
    printf '\n%s\n' "not ok 1 - step callback reentry"
    exit 0
}
printf '\n%s\n' "ok 1 - step callback reentry"
exit 0
