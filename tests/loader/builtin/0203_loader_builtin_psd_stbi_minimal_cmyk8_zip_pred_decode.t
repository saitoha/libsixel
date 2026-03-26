#!/bin/sh
# Verify builtin loader decodes CMYK 8-bit ZIP+prediction with stable image quality.
# Reference generation command (builtin loader):
#   DYLD_LIBRARY_PATH=src/.libs converters/.libs/img2sixel \
#       -L builtin! \
#       tests/data/inputs/formats/stbi_minimal_cmyk8.psd \
#       > tests/data/loader/builtin_expected/psd_cmyk8_raw_builtin_expected.six

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_cmyk8_zip_pred.psd"
reference_six="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_cmyk8_raw_builtin_expected.six"
output_sixel="${ARTIFACT_LOCAL_DIR}/psd_cmyk8_zip_pred_output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR_PSD_CMYK8:-0.995}

run_img2sixel -L builtin! "${input_psd}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin loader failed to decode CMYK 8-bit ZIP+prediction"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_six}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "builtin CMYK 8-bit ZIP+prediction fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin CMYK 8-bit ZIP+prediction keeps MS-SSIM ${lsqa_floor} against expected sixel"
exit 0
