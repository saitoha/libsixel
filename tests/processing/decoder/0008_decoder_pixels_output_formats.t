#!/bin/sh
# Verify sixel_decode_pixels() packed output byte-order hints.
# Policy: docs/decoder/dequantization/README.md
# Policy: docs/decoder/pipeline/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0008_decoder_pixels_output_formats" || {
    echo "not ok 1 - 0008_decoder_pixels_output_formats"
    exit 0
}

echo "ok 1 - 0008_decoder_pixels_output_formats"
exit 0
