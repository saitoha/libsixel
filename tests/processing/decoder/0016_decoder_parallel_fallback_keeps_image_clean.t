#!/bin/sh
# Policy: docs/loader/builtin/sixel.md
# Policy: docs/functionality/decoding-pipeline.md
# Verify failed parallel decode leaves the serial fallback image clean.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0016_decoder_parallel_fallback_keeps_image_clean" || {
    echo "not ok 1 - 0016_decoder_parallel_fallback_keeps_image_clean"
    exit 0
}

echo "ok 1 - 0016_decoder_parallel_fallback_keeps_image_clean"
exit 0
