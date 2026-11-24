#!/bin/sh
# Verify ladder packet allowance.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0132_ladder_packet_allowance" || {
    printf '\n%s\n' "not ok 1 - ladder packet allowance"
    exit 0
}
printf '\n%s\n' "ok 1 - ladder packet allowance"
exit 0
