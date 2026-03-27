#!/bin/sh
# TAP test for forced libjpeg 12-bit JPEG decode quality.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_JPEG-}" = 1 || {
    printf "1..0 # SKIP libjpeg loader is unavailable\n"
    exit 0
}

test "${HAVE_JPEG12_API-}" = 1 || {
    printf "1..0 # SKIP libjpeg 12-bit API is unavailable\n"
    exit 0
}


echo "1..1"
set -v

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_jpeg="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-12bit.jpg"
reference_path="${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake-jpeg-12bit.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L libjpeg! "${input_jpeg}" -o "${output_sixel}" || {
    echo "not ok" 1 - "forced libjpeg 12-bit JPEG decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_path}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "forced libjpeg 12-bit JPEG fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "forced libjpeg 12-bit JPEG keeps MS-SSIM ${lsqa_floor}"
exit 0
