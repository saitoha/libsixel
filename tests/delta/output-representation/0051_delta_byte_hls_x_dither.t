#!/bin/sh
# Byte HLS x-dither captures and compares independent emitted colors without float promotion.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0186_delta_byte_hls_x_dither || {
    echo "not ok 1 - delta byte hls x dither"
    exit 0
}
echo "ok 1 - delta byte hls x dither"
exit 0
