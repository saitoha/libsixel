#!/bin/sh
# Verify explicit raster-attribute trust clipping in sixel_decode_pixels().

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0009_decoder_pixels_trust_raster_clip" || {
    echo "not ok 1 - 0009_decoder_pixels_trust_raster_clip"
    exit 0
}

echo "ok 1 - 0009_decoder_pixels_trust_raster_clip"
exit 0
