#!/bin/sh
# Verify k_undither output with edge protection enabled.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0012_decoder_kundither_edge" || {
    echo "not ok 1 - 0012_decoder_kundither_edge"
    exit 0
}

echo "ok 1 - 0012_decoder_kundither_edge"
exit 0
