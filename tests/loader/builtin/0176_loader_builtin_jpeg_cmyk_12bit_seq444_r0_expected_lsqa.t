#!/bin/sh
# Verify builtin loader decodes 12-bit CMYK JPEG (sequential 4:4:4)
# with stable quality against expected PNM reference.
# Reference generation command (libjpeg tool path):
#   ./converters/jpegref tests/data/inputs/formats/snake-jpeg-12bit-cmyk-seq444.jpg \
#       tests/data/loader/builtin_expected/0015_jpeg_cmyk_12bit_seq444_r0_reference.ppm

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
input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-12bit-cmyk-seq444.jpg"
reference_path="${TOP_SRCDIR}/tests/data/loader/builtin_expected/0015_jpeg_cmyk_12bit_seq444_r0_reference.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/builtin_jpeg_cmyk_12bit_seq444_r0.six"

run_img2sixel -L builtin! "${input_jpeg}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin 12-bit CMYK JPEG decode failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_path}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "builtin 12-bit CMYK JPEG fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin 12-bit CMYK JPEG keeps MS-SSIM ${lsqa_floor} against expected PNM"
exit 0
