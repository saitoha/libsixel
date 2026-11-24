#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Verify sixel_decode_pixels_body() decodes parser-supplied DCS q bodies.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0020_decoder_pixels_body_api" || {
    printf '\n%s\n' "not ok 1 - 0020_decoder_pixels_body_api"
    exit 0
}

printf '\n%s\n' "ok 1 - 0020_decoder_pixels_body_api"
exit 0
