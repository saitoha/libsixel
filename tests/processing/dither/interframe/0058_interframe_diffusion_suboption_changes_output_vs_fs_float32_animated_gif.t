#!/bin/sh
# TAP test ensuring float32 interframe diffusion suboption changes output.

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
    -d fs -p 16 \
    "${input_gif}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin GIF frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

fs_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        --precision=float32 \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "float32 interframe baseline encode failed"
    exit 0
}

atkinson_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        --precision=float32 \
        -d interframe:diffusion=atkinson -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "float32 interframe diffusion=atkinson encode failed"
    exit 0
}

test "${atkinson_output}" != "${fs_output}" || {
    echo "not ok" 1 - "float32 interframe diffusion suboption had no effect"
    exit 0
}

echo "ok" 1 - "float32 interframe diffusion suboption changes output"
exit 0
