#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Policy: docs/decoder/dequantization/README.md
# Verify high color streams bypass the dequantize decode path.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0023_decoder_high_color_dequantize_bypass" || {
    printf '\n%s\n' "not ok 1 - 0023_decoder_high_color_dequantize_bypass"
    exit 0
}

printf '\n%s\n' "ok 1 - 0023_decoder_high_color_dequantize_bypass"
exit 0
