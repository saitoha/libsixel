#!/bin/sh
# Exercise exact-plane 6delta wiring through high-level GPU blue-noise dither.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

# Keep capability initialization and real dispatch in one timed process.
status=0

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    "filter/0028_filter_dither_6delta_bluenoise_gpu_exact_plane" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - 0028 exact-plane GPU blue-noise 6delta"
    exit 0
}

echo "ok 1 - 0028 exact-plane GPU blue-noise 6delta"
exit 0
