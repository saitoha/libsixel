#!/bin/sh
# TAP test ensuring float32 stbn-hash strategy alias matches stbn output.

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
    -d fs -p 16 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

stbn_output=$(
    SIXEL_DITHER_STBN_SOURCE=stbn \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "interframe stbn float32 encode failed"
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
        "${input_apng}"
) || {
    echo "not ok" 1 - "interframe stbn-hash float32 encode failed"
    exit 0
}

test "${stbn_hash_output}" = "${stbn_output}" || {
    echo "not ok" 1 - "float32 stbn-hash output differs from stbn"
    exit 0
}

echo "ok" 1 - "float32 stbn-hash alias matches stbn output"
exit 0
