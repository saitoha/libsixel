#!/bin/sh
# TAP test ensuring temporal strategy suboption overrides env strategy.

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
    SIXEL_DITHER_TEMPORAL_STRATEGY=diffusion \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "float32 temporal diffusion baseline encode failed"
    exit 0
}

env_hash_output=$(
    SIXEL_DITHER_TEMPORAL_STRATEGY=stbn-hash \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "float32 temporal stbn-hash env encode failed"
    exit 0
}

cli_override_output=$(
    SIXEL_DITHER_TEMPORAL_STRATEGY=diffusion \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d interframe:strategy=stbn-hash -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "float32 temporal stbn-hash cli override encode failed"
    exit 0
}

test "${env_hash_output}" != "${diffusion_output}" || {
    echo "not ok" 1 - "float32 stbn-hash env output matched diffusion"
    exit 0
}

test "${cli_override_output}" = "${env_hash_output}" || {
    echo "not ok" 1 - "cli strategy override did not take precedence"
    exit 0
}

echo "ok" 1 - "temporal strategy cli override wins over env"
exit 0
