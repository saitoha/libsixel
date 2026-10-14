#!/bin/sh
# Verify packet buffer flush.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0076_packet_buffer_flush" || {
    echo "not ok 1 - packet buffer flush"
    exit 0
}
echo "ok 1 - packet buffer flush"
exit 0
