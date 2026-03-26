#!/bin/sh
# Verify builtin loader decodes RGB 16-bit ZIP with stable image quality.
# Reference generation command (ImageMagick):
#   magick tests/data/inputs/formats/stbi_minimal_16bpc_raw.psd \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_rgb16_raw_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_16bpc_zip.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_rgb16_raw_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/psd_rgb16_zip_output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}

run_img2sixel -L builtin! "${input_psd}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin loader failed to decode RGB 16-bit ZIP"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "builtin RGB 16-bit ZIP fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin RGB 16-bit ZIP keeps MS-SSIM ${lsqa_floor} against expected PPM"
exit 0
