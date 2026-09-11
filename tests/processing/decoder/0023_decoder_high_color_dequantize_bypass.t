#!/bin/sh
# Policy: docs/functionality/decoding-pipeline.md
# Policy: docs/functionality/dequantization.md
# Verify high color streams bypass the dequantize decode path.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0023_decoder_high_color_dequantize_bypass" || {
    echo "not ok 1 - 0023_decoder_high_color_dequantize_bypass"
    exit 0
}

echo "ok 1 - 0023_decoder_high_color_dequantize_bypass"
exit 0
