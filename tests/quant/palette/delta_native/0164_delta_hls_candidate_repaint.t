#!/bin/sh
# HLS candidate repaint uses decoded HLS RGB rather than directly rounded RGB percentages.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0164_delta_hls_candidate_repaint || {
    echo "not ok 1 - delta hls candidate repaint"
    exit 0
}
echo "ok 1 - delta hls candidate repaint"
exit 0
