#!/bin/sh
# Verify direct parallel repeat overflow falls back before painting.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0019_decoder_parallel_direct_repeat_overflow_fallback" || {
    echo "not ok 1 - 0019_decoder_parallel_direct_repeat_overflow_fallback"
    exit 0
}

echo "ok 1 - 0019_decoder_parallel_direct_repeat_overflow_fallback"
exit 0
