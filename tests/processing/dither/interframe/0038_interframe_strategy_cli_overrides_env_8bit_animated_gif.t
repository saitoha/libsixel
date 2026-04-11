#!/bin/sh
# TAP test ensuring 8bit stbn source option overrides env source.

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

diffusion_output=$(
    SIXEL_DITHER_STBN_SOURCE=diffusion \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "8bit interframe diffusion baseline encode failed"
    exit 0
}

env_pmj_output=$(
    SIXEL_DITHER_STBN_SOURCE=pmj \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d stbn -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "8bit stbn env source=pmj encode failed"
    exit 0
}

cli_override_output=$(
    SIXEL_DITHER_STBN_SOURCE=diffusion \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d stbn:source=pmj -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "8bit stbn cli source=pmj encode failed"
    exit 0
}

test "${env_pmj_output}" != "${diffusion_output}" || {
    echo "not ok" 1 - "8bit pmj env output matched diffusion"
    exit 0
}

test "${cli_override_output}" = "${env_pmj_output}" || {
    echo "not ok" 1 - "8bit cli source override did not take precedence"
    exit 0
}

echo "ok" 1 - "8bit stbn source option overrides env source"
exit 0
