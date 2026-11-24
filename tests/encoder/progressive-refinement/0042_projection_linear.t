#!/bin/sh
# Verify projection linear.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0042_projection_linear" || {
    printf '\n%s\n' "not ok 1 - projection linear"
    exit 0
}
printf '\n%s\n' "ok 1 - projection linear"
exit 0
