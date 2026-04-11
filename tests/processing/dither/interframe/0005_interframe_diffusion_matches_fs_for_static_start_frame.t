#!/bin/sh
# TAP test ensuring interframe diffusion matches fs on a static frame render.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/snake_64.gif"
# Start frame numbers are zero-based, so "-T 0" selects the first frame.

interframe_static=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -S -T 0 \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "interframe static start-frame encode failed"
    exit 0
}

fs_static=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -S -T 0 \
        -d fs -Y direct -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "fs static start-frame encode failed"
    exit 0
}

test "${interframe_static}" = "${fs_static}" || {
    echo "not ok" 1 - "interframe changed static start-frame output"
    exit 0
}

echo "ok" 1 - "interframe matches fs for static start-frame"
exit 0
