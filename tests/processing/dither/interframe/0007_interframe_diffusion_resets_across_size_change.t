#!/bin/sh
# TAP test ensuring interframe diffusion state resets before size-changing input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP webp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_anim="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_anim_12x8.webp"
input_gif="${TOP_SRCDIR}/tests/data/inputs/snake_64.gif"

combined_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 -ldisable \
        -d interframe -p 16 \
        "${input_anim}" "${input_gif}"
) || {
    echo "not ok" 1 - "interframe combined encode failed"
    exit 0
}

animated_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 -ldisable \
        -d interframe -p 16 \
        "${input_anim}"
) || {
    echo "not ok" 1 - "interframe animated encode failed"
    exit 0
}

single_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 -ldisable \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "interframe single encode failed"
    exit 0
}

expected_output="${animated_output}${single_output}"
test "${combined_output}" = "${expected_output}" || {
    echo "not ok" 1 - "size change carried interframe state into next input"
    exit 0
}

echo "ok" 1 - "interframe resets across size change"
exit 0
