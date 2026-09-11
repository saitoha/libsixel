#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Policy: docs/functionality/decoding-pipeline.md
# Verify OR mode stays opaque through the dequantize decode path.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0022_decoder_ormode_dequantize_opaque" || {
    echo "not ok 1 - 0022_decoder_ormode_dequantize_opaque"
    exit 0
}

echo "ok 1 - 0022_decoder_ormode_dequantize_opaque"
exit 0
