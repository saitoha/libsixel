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

input_anim="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_anim_12x8.webp"
combined_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 -ldisable \
        -d interframe -p 16 \
        "${input_anim}" "${input_anim}"
) || {
    echo "not ok" 1 - "interframe two-input encode failed"
    exit 0
}

single_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 -ldisable \
        -d interframe -p 16 \
        "${input_anim}"
) || {
    echo "not ok" 1 - "interframe single-input encode failed"
    exit 0
}

expected_output="${single_output}${single_output}"
test "${combined_output}" = "${expected_output}" || {
    echo "not ok" 1 - "interframe state leaked across input boundary"
    exit 0
}

echo "ok" 1 - "interframe resets between input files"
exit 0
