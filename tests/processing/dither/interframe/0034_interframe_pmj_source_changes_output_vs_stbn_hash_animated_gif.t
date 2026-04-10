#!/bin/sh
# TAP test ensuring 8bit PMJ strategy differs from STBN hash.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_gif="${TOP_SRCDIR}/tests/data/inputs/snake_64.gif"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
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

stbn_hash_output=$(
    SIXEL_DITHER_INTERFRAME_STRATEGY=stbn-hash \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "temporal stbn-hash encode failed"
    exit 0
}

pmj_output=$(
    SIXEL_DITHER_INTERFRAME_STRATEGY=pmj \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "temporal pmj encode failed"
    exit 0
}

test "${pmj_output}" != "${stbn_hash_output}" || {
    echo "not ok" 1 - "8bit pmj output matched stbn-hash"
    exit 0
}

echo "ok" 1 - "8bit pmj output differs from stbn-hash"
exit 0
