#!/bin/sh
# Verify projection smptec.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0043_projection_smptec" || {
    printf '\n%s\n' "not ok 1 - projection smptec"
    exit 0
}
printf '\n%s\n' "ok 1 - projection smptec"
exit 0
