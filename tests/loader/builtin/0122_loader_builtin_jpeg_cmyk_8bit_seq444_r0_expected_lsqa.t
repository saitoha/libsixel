#!/bin/sh
# Verify builtin loader decodes 8-bit CMYK JPEG (sequential 4:4:4)
# with stable quality against a fixed expected PNM reference.
# Reference generation command (ImageMagick):
#   magick tests/data/inputs/formats/snake-jpeg-8bit-cmyk-seq444.jpg \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/0012_jpeg_cmyk_8bit_seq444_r0_reference.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-8bit-cmyk-seq444.jpg"
reference_path="${TOP_SRCDIR}/tests/data/loader/builtin_expected/0012_jpeg_cmyk_8bit_seq444_r0_reference.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/builtin_jpeg_cmyk_8bit_seq444_r0.six"

run_img2sixel -Lbuiltin! "${input_jpeg}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin CMYK 8-bit sequential 4:4:4 JPEG decode failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_path}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "builtin CMYK 8-bit sequential 4:4:4 JPEG fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin CMYK 8-bit sequential 4:4:4 JPEG keeps MS-SSIM ${lsqa_floor} against expected PNM"
exit 0
