#!/bin/sh
# HLS smptec output in linear workspace retains independent wire colors across paint, keep, change and invalidation.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0153_delta_hls_smptec_linear_frames || {
    echo "not ok 1 - delta hls smptec linear frames"
    exit 0
}
echo "ok 1 - delta hls smptec linear frames"
exit 0
