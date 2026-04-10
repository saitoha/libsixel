#!/bin/sh
# TAP test ensuring float32 STBN placeholder stays diffusion-equivalent.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_apng_12x8_rgba_loop2.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    --precision=float32 \
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
    SIXEL_TEMPORAL_STRATEGY=diffusion \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d temporal-diffusion -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "temporal diffusion float32 baseline encode failed"
    exit 0
}

stbn_output=$(
    SIXEL_TEMPORAL_STRATEGY=stbn \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d temporal-diffusion -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "temporal stbn float32 placeholder encode failed"
    exit 0
}

test "${stbn_output}" = "${diffusion_output}" || {
    echo "not ok" 1 - "float32 stbn placeholder differs from diffusion"
    exit 0
}

echo "ok" 1 - "float32 stbn placeholder is diffusion-equivalent"
exit 0
