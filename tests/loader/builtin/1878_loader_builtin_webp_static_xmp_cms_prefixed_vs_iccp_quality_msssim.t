#!/bin/sh
# TAP test comparing static prefixed XMP CMS output against equivalent ICCP output.
# Pair: orientation_xmp_icc_srgb_vp8_static_12x8_psprefix.webp vs
# orientation_embedded_srgb_icc_vp8_static_12x8.webp.

set -eux

test "${HAVE_LCMS2-}" = 1 || {
    printf "1..0 # SKIP lcms2 support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_xmp="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_xmp_icc_srgb_vp8_static_12x8_psprefix.webp"
input_iccp="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_embedded_srgb_icc_vp8_static_12x8.webp"
output_xmp="${ARTIFACT_ROOT}/${0##*/}.xmp.png"
output_iccp="${ARTIFACT_ROOT}/${0##*/}.iccp.png"
lsqa_msg=''

SIXEL_LOADER_BUILTIN_CMS_ENGINE=auto
export SIXEL_LOADER_BUILTIN_CMS_ENGINE
SIXEL_LOADER_BUILTIN_ORIENTATION=off
export SIXEL_LOADER_BUILTIN_ORIENTATION
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -S -L builtin! \
    -o "${output_xmp}" "${input_xmp}" >/dev/null || {
    echo "not ok" 1 - "builtin static prefixed XMP CMS quality decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -S -L builtin! \
    -o "${output_iccp}" "${input_iccp}" >/dev/null || {
    echo "not ok" 1 - "builtin static ICCP quality decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM \
    -b "MS-SSIM:0.98" "${output_iccp}" "${output_xmp}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin static prefixed XMP CMS quality keeps MS-SSIM >= 0.98"
exit 0
