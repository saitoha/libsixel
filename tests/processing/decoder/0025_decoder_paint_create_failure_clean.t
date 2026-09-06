#!/bin/sh
# Verify a partial direct paint launch leaves fallback buffers untouched.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0025_decoder_paint_create_failure_clean" || {
    echo "not ok 1 - 0025_decoder_paint_create_failure_clean"
    exit 0
}

echo "ok 1 - 0025_decoder_paint_create_failure_clean"
exit 0
