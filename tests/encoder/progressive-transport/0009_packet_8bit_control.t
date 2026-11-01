#!/bin/sh
# Verify packet 8bit control.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0077_packet_8bit_control" || {
    echo "not ok 1 - packet 8bit control"
    exit 0
}
echo "ok 1 - packet 8bit control"
exit 0
