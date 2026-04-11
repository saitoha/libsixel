#!/bin/sh
# TAP test ensuring stbn diffusion env is isolated from interframe diffusion.

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

stbn_default_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d stbn -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "8bit stbn default encode failed"
    exit 0
}

stbn_with_interframe_env_output=$(
    SIXEL_DITHER_INTERFRAME_DIFFUSION=atkinson \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d stbn -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "8bit stbn encode with interframe diffusion env failed"
    exit 0
}

stbn_with_stbn_env_output=$(
    SIXEL_DITHER_STBN_DIFFUSION=atkinson \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d stbn -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "8bit stbn encode with stbn diffusion env failed"
    exit 0
}

test "${stbn_with_interframe_env_output}" = "${stbn_default_output}" || {
    echo "not ok" 1 - "stbn output changed by interframe diffusion env"
    exit 0
}

test "${stbn_with_stbn_env_output}" != "${stbn_default_output}" || {
    echo "not ok" 1 - "stbn diffusion env did not affect stbn output"
    exit 0
}

echo "ok" 1 - "stbn diffusion env is isolated from interframe diffusion env"
exit 0
