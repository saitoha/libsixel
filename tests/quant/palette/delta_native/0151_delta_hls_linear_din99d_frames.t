#!/bin/sh
# HLS linear output in din99d workspace retains independent wire colors across paint, keep, change and invalidation.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0151_delta_hls_linear_din99d_frames || {
    echo "not ok 1 - delta hls linear din99d frames"
    exit 0
}
echo "ok 1 - delta hls linear din99d frames"
exit 0
