#!/bin/sh
# TAP test: libtiff colormgmt parity for palette/img_tiff_palette_icc0_wp1_pc1_trc0.tiff with cms=0

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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_tiff="${TOP_SRCDIR}/tests/data/colormgmt/input/tiff/palette/img_tiff_palette_icc0_wp1_pc1_trc0.tiff"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/tiff/palette/img_tiff_palette_icc0_wp1_pc1_trc0.six"
output_six="${ARTIFACT_LOCAL_DIR}/img_tiff_palette_icc0_wp1_pc1_trc0_libtiff_cms0.six"

test -f "${input_tiff}" || {
    echo "not ok" 1 - "missing input fixture: palette/img_tiff_palette_icc0_wp1_pc1_trc0.tiff"
    exit 0
}

test -f "${reference_six}" || {
    echo "not ok" 1 - "missing reference fixture: palette/img_tiff_palette_icc0_wp1_pc1_trc0.six"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibtiff:cms_engine=none! "${input_tiff}" >"${output_six}" || {
    echo "not ok" 1 - "libtiff decode failed with cms=0: palette/img_tiff_palette_icc0_wp1_pc1_trc0"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.998"     "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "palette/img_tiff_palette_icc0_wp1_pc1_trc0 (cms=0): ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libtiff colormgmt matches reference with cms=0: palette/img_tiff_palette_icc0_wp1_pc1_trc0"
exit 0
