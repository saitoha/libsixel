#!/bin/sh
# Verify projection nonfinite.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0045_projection_nonfinite" || {
    printf '\n%s\n' "not ok 1 - projection nonfinite"
    exit 0
}
printf '\n%s\n' "ok 1 - projection nonfinite"
exit 0
