#!/bin/sh
# TAP test ensuring STBN mask source differs from hash source on animation.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_gif="${TOP_SRCDIR}/tests/data/inputs/snake_64.gif"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    -L builtin -ldisable \
    -S -T 0 \
    -d fs -p 16 \
    "${input_gif}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin GIF frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

hash_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin -ldisable \
        -d stbn:source=hash -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "interframe stbn-hash encode failed"
    exit 0
}

mask_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin -ldisable \
        -d stbn:source=mask -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "interframe stbn-mask encode failed"
    exit 0
}

test "${mask_output}" != "${hash_output}" || {
    echo "not ok" 1 - "stbn-mask output matched stbn-hash"
    exit 0
}

echo "ok" 1 - "stbn-mask output differs from stbn-hash"
exit 0
