#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Policy: docs/decoder/dequantization/README.md
# Verify default k_undither output for a 32-color indexed image.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0010_decoder_kundither_default" || {
    printf '\n%s\n' "not ok 1 - 0010_decoder_kundither_default"
    exit 0
}

printf '\n%s\n' "ok 1 - 0010_decoder_kundither_default"
exit 0
