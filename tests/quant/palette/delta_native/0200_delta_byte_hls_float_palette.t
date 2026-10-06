#!/bin/sh
# Legacy indexed HLS history follows the final float palette rather than its byte shadow.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0200_delta_byte_hls_float_palette || {
    echo "not ok 1 - delta byte hls float palette"
    exit 0
}
echo "ok 1 - delta byte hls float palette"
exit 0
