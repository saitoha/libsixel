#!/bin/sh
# TAP test ensuring unknown float32 interframe strategy falls back to diffusion.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    --precision=float32 \
    -L builtin \
    -ldisable \
    -S -T 1 \
    -d fs -p 16 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
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
        "${input_apng}"
) || {
    echo "not ok" 1 - "interframe diffusion float32 baseline encode failed"
    exit 0
}

unknown_output=$(
    SIXEL_DITHER_STBN_SOURCE=unknown-strategy \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "interframe unknown-strategy float32 encode failed"
    exit 0
}

test "${unknown_output}" = "${diffusion_output}" || {
    echo "not ok" 1 - "unknown float32 strategy changed diffusion output"
    exit 0
}

echo "ok" 1 - "unknown float32 strategy falls back to diffusion"
exit 0
