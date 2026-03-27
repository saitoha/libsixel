#!/bin/sh
# Verify builtin loader decodes PSD Gray 32-bit ZIP+Prediction with stable image quality.
# Reference generation command (ImageMagick):
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   magick tests/data/inputs/formats/snake16_gray32_raw.psd \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_gray32_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_gray32_zip_pred.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_gray32_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/psd_gray32_zippred_output.six"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >"${output_sixel}" || {
    echo "not ok" 1 - "PSD Gray 32-bit ZIP+Prediction decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "PSD Gray 32-bit ZIP+Prediction fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "PSD Gray 32-bit ZIP+Prediction keeps MS-SSIM ${lsqa_floor} against expected PPM"
exit 0
