#!/bin/sh
# Verify builtin loader decodes 8-bit lossless JPEG (SOF3) with stable quality
# against a fixed expected PNM reference.
# Reference generation command (ImageMagick):
#   magick tests/data/inputs/formats/snake-jpeg-8bit-lossless.jpg \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/0007_jpeg_rgb_8bit_lossless_r0_reference.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-8bit-lossless.jpg"
reference_path="${TOP_SRCDIR}/tests/data/loader/builtin_expected/0007_jpeg_rgb_8bit_lossless_r0_reference.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/builtin_jpeg_8bit_lossless.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! "${input_jpeg}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin 8-bit lossless JPEG decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_path}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "builtin 8-bit lossless JPEG fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin 8-bit lossless JPEG keeps MS-SSIM ${lsqa_floor} against expected PNM"
exit 0
