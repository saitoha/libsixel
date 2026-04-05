#!/bin/sh
# Verify libpng builtin CMS switches A2B slot by rendering intent.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_mab_a2b012_intent.png"
out_perceptual="${TMPDIR:-/tmp}/libsixel-rgb-a2b012-png-perceptual-$$.six"
out_saturation="${TMPDIR:-/tmp}/libsixel-rgb-a2b012-png-saturation-$$.six"

test -f "${input_png}" || {
    echo "not ok" 1 - "missing input fixture: rgb_mab_a2b012_intent.png"
    exit 0
}

SIXEL_LOADER_CMS_RENDERING_INTENT=perceptual! \
SIXEL_CMS_RENDERING_INTENT=perceptual! \
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:cms_engine=builtin! "${input_png}" >"${out_perceptual}" || {
    echo "not ok" 1 - "libpng builtin cms decode failed: perceptual intent"
    exit 0
}

SIXEL_LOADER_CMS_RENDERING_INTENT=saturation! \
SIXEL_CMS_RENDERING_INTENT=saturation! \
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng:cms_engine=builtin! "${input_png}" >"${out_saturation}" || {
    echo "not ok" 1 - "libpng builtin cms decode failed: saturation intent"
    exit 0
}

cmp -s "${out_perceptual}" "${out_saturation}" && {
    echo "not ok" 1 - "libpng builtin cms intent switch did not change output"
    exit 0
}

echo "ok" 1 - "libpng builtin cms intent switch changes A2B output"
exit 0
