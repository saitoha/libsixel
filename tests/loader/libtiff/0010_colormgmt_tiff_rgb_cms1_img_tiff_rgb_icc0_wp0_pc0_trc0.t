#!/bin/sh
# TAP test: libtiff colormgmt parity for rgb/img_tiff_rgb_icc0_wp0_pc0_trc0.tiff

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

input_tiff="${TOP_SRCDIR}/tests/data/colormgmt/input/tiff/rgb/img_tiff_rgb_icc0_wp0_pc0_trc0.tiff"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/tiff/rgb/img_tiff_rgb_icc0_wp0_pc0_trc0.six"
output_six="${ARTIFACT_LOCAL_DIR}/img_tiff_rgb_icc0_wp0_pc0_trc0_libtiff.six"

test -f "${input_tiff}" || {
    echo "not ok" 1 - "missing input fixture: rgb/img_tiff_rgb_icc0_wp0_pc0_trc0.tiff"
    exit 0
}

test -f "${reference_six}" || {
    echo "not ok" 1 - "missing reference fixture: rgb/img_tiff_rgb_icc0_wp0_pc0_trc0.six"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibtiff:cms_engine=auto! "${input_tiff}" >"${output_six}" || {
    echo "not ok" 1 - "libtiff decode failed: rgb/img_tiff_rgb_icc0_wp0_pc0_trc0"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.997" \
    "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "rgb/img_tiff_rgb_icc0_wp0_pc0_trc0: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libtiff colormgmt matches reference: rgb/img_tiff_rgb_icc0_wp0_pc0_trc0"
exit 0
