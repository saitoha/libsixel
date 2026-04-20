#!/bin/sh
# TAP test ensuring float32 -d interframe ignores STBN source env overrides.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    --precision=float32 \
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
        --precision=float32 \
        -L builtin \
        -ldisable \
        -d interframe -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "float32 interframe default encode failed"
    exit 0
}

env_override_output=$(
    SIXEL_DITHER_STBN_SOURCE=mask \
        ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
            --threads=1 \
            --precision=float32 \
            -L builtin \
            -ldisable \
            -d interframe -p 16 \
            "${input_gif}"
) || {
    echo "not ok" 1 - "float32 interframe encode with env override failed"
    exit 0
}

test "${env_override_output}" = "${default_output}" || {
    echo "not ok" 1 - "float32 interframe output changed by env source override"
    exit 0
}

echo "ok" 1 - "float32 interframe output ignores env source override"
exit 0
