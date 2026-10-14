#!/bin/sh
# Verify packet bounded storage.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0072_packet_bounded_storage" || {
    echo "not ok 1 - packet bounded storage"
    exit 0
}
echo "ok 1 - packet bounded storage"
exit 0
