#!/bin/sh
# TAP test ensuring 8bit strategy=diffusion matches default temporal output.

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

default_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "8bit temporal default encode failed"
    exit 0
}

strategy_diffusion_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d interframe:strategy=diffusion -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "8bit temporal strategy=diffusion encode failed"
    exit 0
}

test "${strategy_diffusion_output}" = "${default_output}" || {
    echo "not ok" 1 - "8bit strategy=diffusion output differs from default"
    exit 0
}

echo "ok" 1 - "8bit strategy=diffusion matches default temporal output"
exit 0
