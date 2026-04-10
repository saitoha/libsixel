#!/bin/sh
# TAP test covering interframe acceptance on float32 precision.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/snake_64.gif"

float32_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -S -T 0 \
        -d interframe -y serpentine -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "interframe float32 with scan option failed"
    exit 0
}

test -n "${float32_output}" || {
    echo "not ok" 1 - "interframe float32 output is empty"
    exit 0
}

echo "ok" 1 - "interframe float32 accepts -y scan option"
exit 0
