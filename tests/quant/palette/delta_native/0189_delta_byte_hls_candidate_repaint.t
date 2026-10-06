#!/bin/sh
# Byte HLS candidate repaint compares the indexed HLS reconstruction rather than pre-wire RGB.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0189_delta_byte_hls_candidate_repaint || {
    echo "not ok 1 - delta byte hls candidate repaint"
    exit 0
}
echo "ok 1 - delta byte hls candidate repaint"
exit 0
