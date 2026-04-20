#!/bin/sh
# TAP test ensuring interframe float32 matches fs on static start frame.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"
# Start frame numbers are zero-based, so "-T 0" selects the first frame.

interframe_static=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -S -T 0 \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "interframe float32 static encode failed"
    exit 0
}

fs_static=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -S -T 0 \
        -d fs -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "fs float32 static start-frame encode failed"
    exit 0
}

test "${interframe_static}" = "${fs_static}" || {
    echo "not ok" 1 - "interframe float32 changed static output"
    exit 0
}

echo "ok" 1 - "interframe float32 matches fs for static start frame"
exit 0
