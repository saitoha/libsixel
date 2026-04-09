#!/bin/sh
# TAP test ensuring temporal diffusion changes animated output versus fs.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
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

temporal_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L libwebp \
        -ldisable \
        -d temporal-diffusion -p 16 \
        "${input_webp}"
) || {
    echo "not ok" 1 - "temporal-diffusion animated encode failed"
    exit 0
}

fs_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L libwebp \
        -ldisable \
        -d fs -Y direct -p 16 \
        "${input_webp}"
) || {
    echo "not ok" 1 - "fs animated encode failed"
    exit 0
}

test "${temporal_output}" != "${fs_output}" || {
    echo "not ok" 1 - "temporal-diffusion did not change animated output"
    exit 0
}

echo "ok" 1 - "temporal-diffusion changes animated output"
exit 0
