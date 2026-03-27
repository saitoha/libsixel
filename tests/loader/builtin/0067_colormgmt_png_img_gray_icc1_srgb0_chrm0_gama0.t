#!/bin/sh
# TAP test: builtin loader colormgmt parity for gray/img_gray_icc1_srgb0_chrm0_gama0.png

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/colormgmt/input/png/gray/img_gray_icc1_srgb0_chrm0_gama0.png"
reference_six="${TOP_SRCDIR}/tests/data/colormgmt/reference/png/gray/img_gray_icc1_srgb0_chrm0_gama0.six"
output_six="${ARTIFACT_LOCAL_DIR}/img_gray_icc1_srgb0_chrm0_gama0_builtin.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=auto! "${input_png}" >"${output_six}" || {
    echo "not ok" 1 - "builtin decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" "${reference_six}" "${output_six}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin colormgmt matches reference: gray/img_gray_icc1_srgb0_chrm0_gama0.png"
exit 0
