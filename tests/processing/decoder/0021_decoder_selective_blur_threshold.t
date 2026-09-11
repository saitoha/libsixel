#!/bin/sh
# Policy: docs/functionality/decoding-pipeline.md
# Policy: docs/functionality/dequantization.md
# Verify selective_blur applies the RGB-distance threshold.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0021_decoder_selective_blur_threshold" || {
    echo "not ok 1 - 0021_decoder_selective_blur_threshold"
    exit 0
}

echo "ok 1 - 0021_decoder_selective_blur_threshold"
exit 0
