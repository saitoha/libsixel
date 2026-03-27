#!/bin/sh
# Verify libjpeg loader decodes 16-bit CMYK lossless JPEG on no-lcms builds with cms=1.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

test "${HAVE_LCMS2-}" = 1 && {
    printf "1..0 # SKIP this case is for no-lcms builds\n"
    exit 0
}

test "${HAVE_JPEG16_API-}" = 1 || {
    printf "1..0 # SKIP libjpeg 16-bit API is unavailable\n"
    exit 0
}


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-16bit-cmyk-lossless.jpg"
reference_path="${TOP_SRCDIR}/tests/data/loader/builtin_expected/0016_jpeg_cmyk_16bit_lossless_r0_reference.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/libjpeg_jpeg_cmyk_16bit_lossless_nolcms_cms1.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libjpeg:cms_engine=auto! "${input_jpeg}" >"${output_sixel}" || {
    echo "not ok" 1 - "libjpeg 16-bit CMYK lossless JPEG decode failed on no-lcms build (cms=1)"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_path}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "libjpeg 16-bit CMYK lossless JPEG on no-lcms build (cms=1) fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libjpeg 16-bit CMYK lossless JPEG on no-lcms build (cms=1) keeps MS-SSIM ${lsqa_floor} against expected PNM"
exit 0
