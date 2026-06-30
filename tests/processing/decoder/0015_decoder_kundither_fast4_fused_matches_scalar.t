#!/bin/sh
# Verify fused fast4 decode+undither output matches scalar output.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0015_decoder_kundither_fast4_fused_matches_scalar" || {
    echo "not ok 1 - 0015_decoder_kundither_fast4_fused_matches_scalar"
    exit 0
}

echo "ok 1 - 0015_decoder_kundither_fast4_fused_matches_scalar"
exit 0
