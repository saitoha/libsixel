#!/bin/sh
# Verify builtin loader decodes Duotone 8-bit ZIP with stable image quality.
# Reference generation command (ImageMagick):
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   magick tests/data/inputs/formats/snake16_duotone8_raw.psd \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_duotone8_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_duotone8_zip.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_duotone8_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/psd_duotone8_zip_output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}

run_img2sixel -L builtin! "${input_psd}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin loader failed to decode Duotone 8-bit ZIP"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "builtin Duotone 8-bit ZIP fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin Duotone 8-bit ZIP keeps MS-SSIM ${lsqa_floor} against expected PPM"
exit 0
