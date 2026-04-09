#!/bin/sh
# TAP test ensuring temporal diffusion output stays stable across thread counts.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_anim_12x8.webp"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    -L libwebp \
    -ldisable \
    -S -T 1 \
    -d fs -Y direct -p 16 \
    "${input_webp}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated libwebp frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

single_thread_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L libwebp \
        -ldisable \
        -d temporal-diffusion -p 16 \
        "${input_webp}"
) || {
    echo "not ok" 1 - "temporal-diffusion single-thread encode failed"
    exit 0
}

multi_thread_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=2 \
        -L libwebp \
        -ldisable \
        -d temporal-diffusion -p 16 \
        "${input_webp}"
) || {
    echo "not ok" 1 - "temporal-diffusion multi-thread encode failed"
    exit 0
}

test "${multi_thread_output}" = "${single_thread_output}" || {
    echo "not ok" 1 - "temporal-diffusion output changed across thread counts"
    exit 0
}

echo "ok" 1 - "temporal-diffusion output is stable across thread counts"
exit 0
