#!/bin/sh
# Verify builtin loader decodes PSD CMYK 16-bit ZIP with stable image quality.
# Fixture generation command:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
# Reference generation command (ImageMagick):
#   magick tests/data/inputs/formats/snake16_cmyk16_raw.psd \
#       -colorspace sRGB -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_cmyk16_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_cmyk16_zip.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_cmyk16_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/psd_cmyk16_zip_output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.99}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >"${output_sixel}" || {
    echo "not ok" 1 - "PSD CMYK 16-bit ZIP decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear -P float32 -b "MS-SSIM:${lsqa_floor}" \
    "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "PSD CMYK 16-bit ZIP fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "PSD CMYK 16-bit ZIP keeps MS-SSIM ${lsqa_floor} against expected PPM"
exit 0
