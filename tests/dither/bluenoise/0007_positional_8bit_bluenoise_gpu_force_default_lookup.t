#!/bin/sh
# TAP test covering forced GPU bluenoise with the default lookup policy.
#
# The compatibility root resolves AUTO to the owned exhaustive GPU lookup.
# Probe an explicit policy so a broken AUTO resolver fails this test rather
# than being mistaken for unavailable hardware.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

probe_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
input_image="${TOP_SRCDIR}/images/vimperator3.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_GPU_POLICY=force \
        -~ none -d none -p 16 -o /dev/null "${probe_image}" || {
    printf "1..0 # SKIP forced GPU palette apply is unavailable\n";
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_ENCODER_THREADING_POLICY=greedy --env SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
        --env SIXEL_GPU_POLICY=force \
        --env SIXEL_DITHER_BLUENOISE_STRENGTH=0.4 \
        -d bluenoise -p 256 -o /dev/null "${input_image}" || {
    echo "not ok" 1 - "serial forced GPU bluenoise default lookup failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_ENCODER_THREADING_POLICY=greedy --env SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=6 \
        --env SIXEL_GPU_POLICY=force \
        --env SIXEL_DITHER_BLUENOISE_STRENGTH=0.4 \
        -d bluenoise -p 256 -o /dev/null "${input_image}" || {
    echo "not ok" 1 - "parallel forced GPU bluenoise default lookup failed"
    exit 0
}

echo "ok" 1 - "forced GPU bluenoise accepts default lookup policy"
exit 0
