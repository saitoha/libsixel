#!/bin/sh
# TAP test: builtin loader colormgmt parity (nolcms, enable_cms=1) for gray/img_gray_icc1_srgb1_chrm1_gama1.png

set -eux

test "${HAVE_LCMS2-}" != 1 || {
    printf "1..0 # SKIP lcms2 support is enabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/colormgmt/input/png/gray/img_gray_icc1_srgb1_chrm1_gama1.png"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/png/gray/img_gray_icc1_srgb1_chrm1_gama1.six"
output_six="${ARTIFACT_LOCAL_DIR}/img_gray_icc1_srgb1_chrm1_gama1_builtin_nolcms_enable_cms1.six"

if [ ! -f "${input_png}" ] || [ ! -f "${reference_six}" ]; then
    echo "not ok" 1 - "missing test fixture"
    exit 0
fi

run_img2sixel -Lbuiltin:cms=1! "${input_png}" >"${output_six}" || {
    echo "not ok" 1 - "builtin decode failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin colormgmt (nolcms, enable_cms=1) matches reference: gray/img_gray_icc1_srgb1_chrm1_gama1.png"
exit 0
