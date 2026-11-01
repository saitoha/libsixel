#!/bin/sh
# HLS kept-color skip uses the retained displayed gray in FS diffusion.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0171_delta_hls_residual_skip || {
    echo "not ok 1 - delta hls residual skip"
    exit 0
}
echo "ok 1 - delta hls residual skip"
exit 0
