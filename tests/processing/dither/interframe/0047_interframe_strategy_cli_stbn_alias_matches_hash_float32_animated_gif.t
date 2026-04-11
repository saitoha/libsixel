#!/bin/sh
# TAP test ensuring float32 CLI stbn alias matches stbn-hash output.

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

stbn_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d stbn -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "float32 interframe stbn encode failed"
    exit 0
}

stbn_hash_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d stbn:source=hash -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "float32 interframe source=hash encode failed"
    exit 0
}

test "${stbn_hash_output}" = "${stbn_output}" || {
    echo "not ok" 1 - "float32 CLI stbn alias differs from stbn-hash"
    exit 0
}

echo "ok" 1 - "float32 CLI stbn alias matches stbn-hash output"
exit 0
