#!/bin/sh
# TAP test ensuring CLI strength overrides env for 8bit interframe.

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

diffusion_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "interframe diffusion GIF encode failed"
    exit 0
}

env_zero_output=$(
    SIXEL_DITHER_STBN_SOURCE=stbn-mask \
    SIXEL_DITHER_STBN_STRENGTH=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "env-based interframe stbn-mask noise=0 encode failed"
    exit 0
}

test "${env_zero_output}" = "${diffusion_output}" || {
    echo "not ok" 1 - "env noise=0 did not match interframe diffusion output"
    exit 0
}

cli_override_output=$(
    SIXEL_DITHER_STBN_SOURCE=diffusion \
    SIXEL_DITHER_STBN_STRENGTH=0 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d stbn:source=mask:strength=0.50 -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "cli interframe strength override encode failed"
    exit 0
}

test "${cli_override_output}" != "${diffusion_output}" || {
    echo "not ok" 1 - "cli strength override did not take effect"
    exit 0
}

echo "ok" 1 - "cli strength override wins over env settings"
exit 0
