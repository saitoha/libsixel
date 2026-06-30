#!/bin/sh
# Verify fast4 parallel k_undither output matches scalar output.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0014_decoder_kundither_fast4_parallel_matches_scalar" || {
    echo "not ok 1 - 0014_decoder_kundither_fast4_parallel_matches_scalar"
    exit 0
}

echo "ok 1 - 0014_decoder_kundither_fast4_parallel_matches_scalar"
exit 0
