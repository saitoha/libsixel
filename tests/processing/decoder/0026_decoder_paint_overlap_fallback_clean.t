#!/bin/sh
# Verify overlapping direct paint row ranges fall back before publication.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0026_decoder_paint_overlap_fallback_clean" || {
    echo "not ok 1 - 0026_decoder_paint_overlap_fallback_clean"
    exit 0
}

echo "ok 1 - 0026_decoder_paint_overlap_fallback_clean"
exit 0
