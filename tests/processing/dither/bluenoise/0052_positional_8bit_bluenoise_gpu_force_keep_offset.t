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

probe_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_GPU_POLICY=force \
        -d none -p 16 --lookup-policy=none -o /dev/null "${probe_image}" || {
    printf "1..0 # SKIP forced GPU palette apply is unavailable\n";
    exit 0;
}

echo "1..1"
set -v

SIXEL_THREADS=1 \
SIXEL_DITHER_BLUENOISE_STRENGTH=0.4 \
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/bluenoise/0052_keep_offset >/dev/null || {
    echo "not ok 1 - forced GPU bluenoise keep-offset encode failed"
    exit 0
}

echo "ok 1 - forced GPU bluenoise accepts keep-offset accumulation"
exit 0
