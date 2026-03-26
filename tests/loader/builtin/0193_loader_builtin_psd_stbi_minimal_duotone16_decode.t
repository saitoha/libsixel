#!/bin/sh
# Verify builtin loader decodes duotone 16-bit raw with stable image quality.
# Reference generation command (ImageMagick):
#   magick tests/data/inputs/formats/stbi_minimal_duotone16.psd \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_duotone16_raw_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_duotone16.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_duotone16_raw_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/psd_duotone16_raw_output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}

run_img2sixel -L builtin! "${input_psd}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin loader failed to decode duotone 16-bit raw"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "builtin duotone 16-bit raw fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin duotone 16-bit raw keeps MS-SSIM ${lsqa_floor} against expected PPM"
exit 0
