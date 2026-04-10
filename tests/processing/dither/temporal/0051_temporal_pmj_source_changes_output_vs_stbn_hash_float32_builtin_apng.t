#!/bin/sh
# TAP test ensuring float32 PMJ strategy differs from STBN hash on small APNG.

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

hash_output=$(
    SIXEL_DITHER_TEMPORAL_STRATEGY=stbn-hash \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d temporal-diffusion -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "temporal stbn-hash float32 APNG encode failed"
    exit 0
}

pmj_output=$(
    SIXEL_DITHER_TEMPORAL_STRATEGY=pmj \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d temporal-diffusion -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "temporal pmj float32 APNG encode failed"
    exit 0
}

test "${pmj_output}" != "${hash_output}" || {
    echo "not ok" 1 - "float32 pmj APNG output matched stbn-hash"
    exit 0
}

echo "ok" 1 - "float32 pmj APNG output differs from stbn-hash"
exit 0
