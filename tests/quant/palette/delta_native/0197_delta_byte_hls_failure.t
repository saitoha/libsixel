#!/bin/sh
# Byte HLS failure clears the borrowed comparison view before reuse.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0197_delta_byte_hls_failure || {
    echo "not ok 1 - delta byte hls failure"
    exit 0
}
echo "ok 1 - delta byte hls failure"
exit 0
