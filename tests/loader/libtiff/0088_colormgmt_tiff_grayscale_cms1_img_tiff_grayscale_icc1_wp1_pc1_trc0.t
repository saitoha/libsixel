#!/bin/sh
# TAP test: libtiff colormgmt parity for grayscale/img_tiff_grayscale_icc1_wp1_pc1_trc0.tiff

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_tiff="${TOP_SRCDIR}/tests/data/colormgmt/input/tiff/grayscale/img_tiff_grayscale_icc1_wp1_pc1_trc0.tiff"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/tiff/grayscale/img_tiff_grayscale_icc1_wp1_pc1_trc0.six"
output_six="${ARTIFACT_LOCAL_DIR}/img_tiff_grayscale_icc1_wp1_pc1_trc0_libtiff.six"

test -f "${input_tiff}" || {
    echo "not ok" 1 - "missing input fixture: grayscale/img_tiff_grayscale_icc1_wp1_pc1_trc0.tiff"
    exit 0
}

test -f "${reference_six}" || {
    echo "not ok" 1 - "missing reference fixture: grayscale/img_tiff_grayscale_icc1_wp1_pc1_trc0.six"
    exit 0
}

run_img2sixel -Llibtiff:cms_engine=auto! "${input_tiff}" >"${output_six}" || {
    echo "not ok" 1 - "libtiff decode failed: grayscale/img_tiff_grayscale_icc1_wp1_pc1_trc0"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.9977"     "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "grayscale/img_tiff_grayscale_icc1_wp1_pc1_trc0: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libtiff colormgmt matches reference: grayscale/img_tiff_grayscale_icc1_wp1_pc1_trc0"
exit 0
