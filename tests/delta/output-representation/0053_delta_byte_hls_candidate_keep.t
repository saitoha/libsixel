#!/bin/sh
# Byte HLS candidate keep compares the indexed HLS reconstruction rather than pre-wire RGB.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0188_delta_byte_hls_candidate_keep || {
    echo "not ok 1 - delta byte hls candidate keep"
    exit 0
}
echo "ok 1 - delta byte hls candidate keep"
exit 0
