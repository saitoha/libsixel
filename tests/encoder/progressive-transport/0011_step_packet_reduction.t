#!/bin/sh
# Verify step packet reduction.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0079_step_packet_reduction" || {
    printf '\n%s\n' "not ok 1 - step packet reduction"
    exit 0
}
printf '\n%s\n' "ok 1 - step packet reduction"
exit 0
