#!/bin/sh
# TAP test ensuring interframe diffusion state is reset between input files.

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

input_anim="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-loop2-min.webp"
combined_cksum=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 -ldisable \
        -d interframe -p 16 \
        "${input_anim}" "${input_anim}" | cksum
) || {
    echo "not ok" 1 - "interframe two-input encode failed"
    exit 0
}

expected_cksum=$(
    {
        ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
            --threads=1 -ldisable \
            -d interframe -p 16 \
            "${input_anim}"
        ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
            --threads=1 -ldisable \
            -d interframe -p 16 \
            "${input_anim}"
    } | cksum
) || {
    echo "not ok" 1 - "interframe expected stream encode failed"
    exit 0
}

test "${combined_cksum}" = "${expected_cksum}" || {
    echo "not ok" 1 - "interframe state leaked across input boundary"
    exit 0
}

echo "ok" 1 - "interframe resets between input files"
exit 0
