#!/bin/sh
# AUTO must use the origin-aware CPU path for an offset retained plane.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}


echo "1..1"
set -v

# Initialize the capability prerequisite and dispatch in one timed process.
status=0

SIXEL_GPU_PALETTE_THRESHOLD=1 \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    "filter/0029_filter_dither_6delta_bluenoise_gpu_auto_fallback" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - 0029 AUTO offset-plane CPU fallback"
    exit 0
}

echo "ok 1 - 0029 AUTO offset-plane CPU fallback"
exit 0
