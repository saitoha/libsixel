#!/bin/sh
# TAP test covering forced GPU bluenoise with the default lookup policy.
#
# The Metal palette-apply kernel performs its own direct palette scan.  FORCE
# mode must therefore bypass CPU lookup-policy choices such as the default
# 5bit path; otherwise terminal clients that only set SIXEL_GPU_POLICY=force
# fail before they can present the first frame.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

probe_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
input_image="${TOP_SRCDIR}/images/vimperator3.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_GPU_POLICY=force \
        -d none -p 16 --lookup-policy=none -o /dev/null "${probe_image}" || {
    printf "1..0 # SKIP forced GPU palette apply is unavailable\n";
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=1 \
        --env SIXEL_GPU_POLICY=force \
        --env SIXEL_DITHER_BLUENOISE_STRENGTH=0.4 \
        -d bluenoise -p 256 -o /dev/null "${input_image}" || {
    echo "not ok" 1 - "serial forced GPU bluenoise default lookup failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=6 \
        --env SIXEL_GPU_POLICY=force \
        --env SIXEL_DITHER_BLUENOISE_STRENGTH=0.4 \
        -d bluenoise -p 256 -o /dev/null "${input_image}" || {
    echo "not ok" 1 - "parallel forced GPU bluenoise default lookup failed"
    exit 0
}

echo "ok" 1 - "forced GPU bluenoise accepts default lookup policy"
exit 0
