#!/bin/sh
# TAP test: libpng loader colormgmt parity for rgb/img_rgb_icc0_srgb0_chrm0_gama1.png

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/colormgmt/input/png/rgb/img_rgb_icc0_srgb0_chrm0_gama1.png"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/png/rgb/img_rgb_icc0_srgb0_chrm0_gama1.six"
output_six="${ARTIFACT_LOCAL_DIR}/img_rgb_icc0_srgb0_chrm0_gama1_libpng.six"

test -f "${input_png}" || {
    echo "not ok" 1 - "missing test fixture"
    exit 0
}

test -f "${reference_six}" || {
    echo "not ok" 1 - "missing test fixture"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibpng:cms_engine=auto! "${input_png}" >"${output_six}" || {
    echo "not ok" 1 - "libpng decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libpng colormgmt matches reference: rgb/img_rgb_icc0_srgb0_chrm0_gama1.png"
exit 0
