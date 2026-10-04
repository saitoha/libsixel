#!/bin/sh
# Reference: original UINT16 ColorMap, plus stored ICC equations for cms1.
# No encoder or CMM is used to generate the unquantized reference.
# TAP test: libtiff colormgmt parity for palette/img_tiff_palette_icc1_wp1_pc1_trc0.tiff

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_tiff="${TOP_SRCDIR}/tests/data/colormgmt/input/tiff/palette/img_tiff_palette_icc1_wp1_pc1_trc0.tiff"
reference_image="${TOP_SRCDIR}/tests/data/colormgmt/reference/tiff/palette/colormap-icc-srgb.ppm"
output_six="${ARTIFACT_LOCAL_DIR}/img_tiff_palette_icc1_wp1_pc1_trc0_libtiff.six"

test -f "${input_tiff}" || {
    echo "not ok" 1 - "missing input fixture: palette/img_tiff_palette_icc1_wp1_pc1_trc0.tiff"
    exit 0
}

test -f "${reference_image}" || {
    echo "not ok" 1 - "missing reference fixture: palette/colormap-icc-srgb.ppm"
    exit 0
}

trace=$(set +xv; SIXEL_TRACE_TOPIC=loader,loader_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibtiff:cms_engine=auto! \
    "${input_tiff}" 2>&1 >"${output_six}") || {
    echo "not ok" 1 - "libtiff decode failed: palette/img_tiff_palette_icc1_wp1_pc1_trc0"
    exit 0
}

# The perceptual threshold alone can accept an unmanaged approximation.
# Require an actual successful CMS application before comparing the image.
case "${trace}" in *"LSXCMS2|event=apply|"*"|src=4|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - managed palette did not reach a successful CMS apply"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.998"     "${reference_image}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "palette/img_tiff_palette_icc1_wp1_pc1_trc0: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libtiff colormgmt matches reference: palette/img_tiff_palette_icc1_wp1_pc1_trc0"
exit 0
