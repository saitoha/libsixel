#!/bin/sh
# HLS kept-color diffuse uses the retained displayed gray in FS diffusion.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0170_delta_hls_residual_diffuse || {
    echo "not ok 1 - delta hls residual diffuse"
    exit 0
}
echo "ok 1 - delta hls residual diffuse"
exit 0
