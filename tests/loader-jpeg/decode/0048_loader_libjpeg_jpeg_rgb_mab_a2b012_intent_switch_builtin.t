#!/bin/sh
# Verify libjpeg builtin CMS switches A2B slot by rendering intent.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_jpeg="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_mab_a2b012_intent.jpg"
out_perceptual="${TMPDIR:-/tmp}/libsixel-rgb-a2b012-jpeg-perceptual-$$.six"
out_saturation="${TMPDIR:-/tmp}/libsixel-rgb-a2b012-jpeg-saturation-$$.six"

test -f "${input_jpeg}" || {
    echo "not ok" 1 - "missing input fixture: rgb_mab_a2b012_intent.jpg"
    exit 0
}

SIXEL_LOADER_CMS_RENDERING_INTENT=perceptual! \
SIXEL_CMS_RENDERING_INTENT=perceptual! \
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibjpeg:cms_engine=builtin! "${input_jpeg}" >"${out_perceptual}" || {
    echo "not ok" 1 - "libjpeg builtin cms decode failed: perceptual intent"
    exit 0
}

SIXEL_LOADER_CMS_RENDERING_INTENT=saturation! \
SIXEL_CMS_RENDERING_INTENT=saturation! \
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibjpeg:cms_engine=builtin! "${input_jpeg}" >"${out_saturation}" || {
    echo "not ok" 1 - "libjpeg builtin cms decode failed: saturation intent"
    exit 0
}

cmp -s "${out_perceptual}" "${out_saturation}" && {
    echo "not ok" 1 - "libjpeg builtin cms intent switch did not change output"
    exit 0
}

echo "ok" 1 - "libjpeg builtin cms intent switch changes A2B output"
exit 0
