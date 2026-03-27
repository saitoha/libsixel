#!/bin/sh
# Verify builtin loader decodes Bitmap 1-bit ZIP with stable image quality.
# Fixture generation command:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
# Reference generation command (ImageMagick):
#   magick tests/data/inputs/formats/snake16_bitmap1_raw.psd \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_bitmap1_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_bitmap1_zip.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_bitmap1_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/psd_bitmap1_zip_output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin loader failed to decode Bitmap 1-bit ZIP"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear -b "MS-SSIM:${lsqa_floor}" \
    "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "builtin Bitmap 1-bit ZIP fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin Bitmap 1-bit ZIP keeps MS-SSIM ${lsqa_floor} against expected PPM"
exit 0
