#!/bin/sh
# Verify k_undither output with a non-default similarity bias.
# Policy: docs/decoder/dequantization/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0011_decoder_kundither_similarity" || {
    printf '\n%s\n' "not ok 1 - 0011_decoder_kundither_similarity"
    exit 0
}

printf '\n%s\n' "ok 1 - 0011_decoder_kundither_similarity"
exit 0
