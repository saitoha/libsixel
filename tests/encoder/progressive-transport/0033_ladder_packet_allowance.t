#!/bin/sh
# Verify ladder packet allowance.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0132_ladder_packet_allowance" || {
    echo "not ok 1 - ladder packet allowance"
    exit 0
}
echo "ok 1 - ladder packet allowance"
exit 0
