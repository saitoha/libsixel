#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Policy: docs/decoder/dequantization/README.md
# Verify selective_blur applies the RGB-distance threshold.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0021_decoder_selective_blur_threshold" || {
    printf '\n%s\n' "not ok 1 - 0021_decoder_selective_blur_threshold"
    exit 0
}

printf '\n%s\n' "ok 1 - 0021_decoder_selective_blur_threshold"
exit 0
