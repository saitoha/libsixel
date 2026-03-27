#!/bin/sh
# Verify libjpeg loader behavior without lcms2 for embedded-ICC JPEG (cms=1).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

test "${HAVE_LCMS2-}" != 1 || {
    printf "1..0 # SKIP lcms2 support is enabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-embedded-esrgb.jpg"
reference_path="${TOP_SRCDIR}/tests/data/loader/builtin_expected/0002_snake_64_embedded_esrgb_converted_srgb_noicc.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/libjpeg_jpeg_embedded_esrgb_nolcms_cms1.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libjpeg:cms_engine=auto! "${input_jpeg}" >"${output_sixel}" || {
    echo "not ok" 1 - "libjpeg embedded-ICC JPEG decode failed (nolcms, cms=1)"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_path}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "libjpeg embedded-ICC JPEG (nolcms, cms=1) fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libjpeg embedded-ICC JPEG (nolcms, cms=1) keeps MS-SSIM ${lsqa_floor} against expected PNM"
exit 0
