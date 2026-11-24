#!/bin/sh
# Verify projection display p3.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0044_projection_display_p3" || {
    printf '\n%s\n' "not ok 1 - projection display p3"
    exit 0
}
printf '\n%s\n' "ok 1 - projection display p3"
exit 0
