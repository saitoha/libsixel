#!/bin/sh
# Verify k_undither output with a non-default similarity bias.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0011_decoder_kundither_similarity" || {
    echo "not ok 1 - 0011_decoder_kundither_similarity"
    exit 0
}

echo "ok 1 - 0011_decoder_kundither_similarity"
exit 0
