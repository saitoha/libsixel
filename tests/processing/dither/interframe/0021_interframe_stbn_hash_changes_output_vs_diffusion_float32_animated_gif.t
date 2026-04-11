#!/bin/sh
# TAP test ensuring float32 STBN hash differs from diffusion on animation.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_gif="${TOP_SRCDIR}/tests/data/inputs/snake_64.gif"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    --precision=float32 \
    -L builtin \
    -ldisable \
    -S -T 0 \
    -d fs -Y direct -p 16 \
    "${input_gif}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin GIF frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

diffusion_output=$(
    SIXEL_DITHER_STBN_SOURCE=diffusion \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "interframe diffusion float32 baseline encode failed"
    exit 0
}

stbn_hash_output=$(
    SIXEL_DITHER_STBN_SOURCE=stbn-hash \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "interframe stbn-hash float32 encode failed"
    exit 0
}

test "${stbn_hash_output}" != "${diffusion_output}" || {
    echo "not ok" 1 - "float32 stbn-hash output matched diffusion"
    exit 0
}

echo "ok" 1 - "float32 stbn-hash output differs from diffusion"
exit 0
