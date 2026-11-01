#!/bin/sh
# Verify packet hls fields.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0070_packet_hls_fields" || {
    echo "not ok 1 - packet hls fields"
    exit 0
}
echo "ok 1 - packet hls fields"
exit 0
