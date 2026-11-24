#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Verify explicit raster-attribute trust clipping in sixel_decode_pixels().

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0009_decoder_pixels_trust_raster_clip" || {
    printf '\n%s\n' "not ok 1 - 0009_decoder_pixels_trust_raster_clip"
    exit 0
}

printf '\n%s\n' "ok 1 - 0009_decoder_pixels_trust_raster_clip"
exit 0
