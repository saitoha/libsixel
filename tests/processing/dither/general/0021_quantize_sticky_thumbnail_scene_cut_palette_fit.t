#!/bin/sh
# TAP test ensuring sticky scene-cut sees thumbnail palette mismatch.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_gray="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-gray.png"
input_rgb="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png"
default_output=''
locked_output=''

echo "1..1"
set -v
set +xv

default_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qsticky:swap_limit=0 \
        -d fs -p 16 \
        "${input_gray}" "${input_rgb}"
) || {
    echo "not ok" 1 - "sticky default scene-cut encode failed"
    exit 0
}

locked_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qsticky:swap_limit=0:scene_cut_threshold=1.0 \
        -d fs -p 16 \
        "${input_gray}" "${input_rgb}"
) || {
    echo "not ok" 1 - "sticky high-threshold encode failed"
    exit 0
}

set -xv

test "${default_output}" != "${locked_output}" || {
    echo "not ok" 1 - "sticky thumbnail palette mismatch did not reset"
    exit 0
}

echo "ok" 1 - "sticky thumbnail palette mismatch resets palette"
exit 0
