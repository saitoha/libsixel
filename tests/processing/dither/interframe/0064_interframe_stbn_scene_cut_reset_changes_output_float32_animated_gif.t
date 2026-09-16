#!/bin/sh
# TAP test ensuring stbn scene_cut_reset changes float32 animated gif output.

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
    --precision=float32 \
    -S -T 0 \
    -d fs -p 16 \
    "${input_gif}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP float32 animated builtin GIF frame path unavailable\n"
    exit 0
}

echo "1..1"
set -v

base_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        --precision=float32 \
        -d stbn:source=pmj -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "float32 stbn base encode failed"
    exit 0
}

scene_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        --precision=float32 \
        -d stbn:source=pmj:scene_cut_reset=1 -p 16 \
        "${input_gif}"
) || {
    echo "not ok" 1 - "float32 stbn scene_cut_reset encode failed"
    exit 0
}

test "${scene_output}" != "${base_output}" || {
    echo "not ok" 1 - "float32 stbn scene_cut_reset did not change output"
    exit 0
}

echo "ok" 1 - "float32 stbn scene_cut_reset changes output"
exit 0
