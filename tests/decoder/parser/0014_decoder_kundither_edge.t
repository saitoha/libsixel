#!/bin/sh
# Verify k_undither output with edge protection enabled.
# Policy: docs/decoder/dequantization/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0012_decoder_kundither_edge" || {
    printf '\n%s\n' "not ok 1 - 0012_decoder_kundither_edge"
    exit 0
}

printf '\n%s\n' "ok 1 - 0012_decoder_kundither_edge"
exit 0
