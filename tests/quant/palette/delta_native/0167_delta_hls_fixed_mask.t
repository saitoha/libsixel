#!/bin/sh
# HLS fixed palettes preserve all 256 slots and mask state across frames.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0167_delta_hls_fixed_mask || {
    echo "not ok 1 - delta hls fixed mask"
    exit 0
}
echo "ok 1 - delta hls fixed mask"
exit 0
