#!/bin/sh
# TAP test covering forced GPU bluenoise with transparent keep accumulation.
#
# alpha-policy=keep with transparent-offset emits P2=1 padding without
# needing a 6delta retained RGB plane.  FORCE mode must still be able to
# exercise the GPU path for terminal video clients that prefer full-paint GPU
# output over CPU fallback.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0;
}

echo "1..1"
set -v

# Keep capability initialization and real dispatch in one timed process.
status=0

SIXEL_THREADING_POLICY=1 \
SIXEL_DITHER_BLUENOISE_STRENGTH=0.4 \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    dither/bluenoise/0052_keep_offset >/dev/null || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - forced GPU bluenoise keep-offset encode failed"
    exit 0
}

echo "ok 1 - forced GPU bluenoise accepts keep-offset accumulation"
exit 0
