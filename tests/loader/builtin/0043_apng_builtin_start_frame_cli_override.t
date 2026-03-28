#!/bin/sh
# TAP test: builtin APNG CLI start frame override wins over env input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png"

frame0_six=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle \
    --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=0" \
    -Lbuiltin! -S "${input_png}") || {
    echo "not ok" 1 - "APNG decode with env start frame 0 failed"
    exit 0
}

frame1_six=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle \
    --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=1" \
    -Lbuiltin! -S "${input_png}") || {
    echo "not ok" 1 - "APNG decode with env start frame 1 failed"
    exit 0
}

override_six=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle \
    --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=0" \
    -T 1 -Lbuiltin! -S "${input_png}") || {
    echo "not ok" 1 - "APNG decode with -T override failed"
    exit 0
}

test "${override_six}" = "${frame1_six}" || {
    echo "not ok" 1 - "-T output does not match env start frame 1"
    exit 0
}

test "${override_six}" != "${frame0_six}" || {
    echo "not ok" 1 - "-T output still matches env start frame 0"
    exit 0
}

echo "ok" 1 - "builtin APNG -T start frame overrides env value"
exit 0
