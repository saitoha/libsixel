#!/bin/sh
# TAP test: libtiff colormgmt parity for grayscale/img_tiff_grayscale_icc0_wp0_pc1_trc1.tiff with cms=0

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

input_tiff="${TOP_SRCDIR}/tests/data/colormgmt/input/tiff/grayscale/img_tiff_grayscale_icc0_wp0_pc1_trc1.tiff"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/tiff/grayscale/img_tiff_grayscale_icc0_wp0_pc1_trc1.six"
output_six="${ARTIFACT_LOCAL_DIR}/img_tiff_grayscale_icc0_wp0_pc1_trc1_libtiff_cms0.six"

test -f "${input_tiff}" || {
    echo "not ok" 1 - "missing input fixture: grayscale/img_tiff_grayscale_icc0_wp0_pc1_trc1.tiff"
    exit 0
}

test -f "${reference_six}" || {
    echo "not ok" 1 - "missing reference fixture: grayscale/img_tiff_grayscale_icc0_wp0_pc1_trc1.six"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibtiff:cms_engine=none! "${input_tiff}" >"${output_six}" || {
    echo "not ok" 1 - "libtiff decode failed with cms=0: grayscale/img_tiff_grayscale_icc0_wp0_pc1_trc1"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.999"     "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "grayscale/img_tiff_grayscale_icc0_wp0_pc1_trc1 (cms=0): ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libtiff colormgmt matches reference with cms=0: grayscale/img_tiff_grayscale_icc0_wp0_pc1_trc1"
exit 0
