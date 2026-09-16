#!/bin/sh
# TAP test ensuring 8bit stbn defaults to diffusion=none.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

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

default_output=$(
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

none_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -d stbn:diffusion=none -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "8bit stbn diffusion=none encode failed"
    exit 0
}

test "${default_output}" = "${none_output}" || {
    echo "not ok" 1 - "8bit stbn default did not match diffusion=none"
    exit 0
}

echo "ok" 1 - "8bit stbn default matches diffusion=none"
exit 0
