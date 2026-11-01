#!/bin/sh
# Byte HLS bluenoise captures and compares independent emitted colors without float promotion.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0187_delta_byte_hls_bluenoise || {
    echo "not ok 1 - delta byte hls bluenoise"
    exit 0
}
echo "ok 1 - delta byte hls bluenoise"
exit 0
