#!/bin/sh
# TAP test ensuring stbn-mask strength=0 matches diffusion on APNG.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_apng_12x8_rgba_loop2.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    -L builtin \
    -ldisable \
    -S -T 1 \
    -d fs -Y direct -p 16 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

diffusion_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "interframe diffusion APNG encode failed"
    exit 0
}

mask_zero_output=$(
    SIXEL_DITHER_STBN_SOURCE=stbn-mask \
    SIXEL_DITHER_STBN_STRENGTH=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "interframe stbn-mask strength=0 APNG encode failed"
    exit 0
}

test "${mask_zero_output}" = "${diffusion_output}" || {
    echo "not ok" 1 - "stbn-mask strength=0 output differed from diffusion"
    exit 0
}

echo "ok" 1 - "stbn-mask strength=0 matches diffusion output"
exit 0
