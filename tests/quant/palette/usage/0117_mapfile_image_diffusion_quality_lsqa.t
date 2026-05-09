#!/bin/sh
# Verify image mapfiles still apply default diffusion to source pixels.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test -x "${LSQA_PATH-}" || {
    printf "1..0 # SKIP lsqa is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
mapfile="${TOP_SRCDIR}/images/map64.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "${input_image}" -m "${mapfile}" |
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m ms-ssim -dk \
        -b "MS-SSIM:0.95" "${input_image}" >/dev/null || {
    echo "not ok" 1 - "mapfile default diffusion fell below MS-SSIM 0.95"
    exit 0
}

echo "ok" 1 - "mapfile default diffusion quality is preserved"

exit 0
