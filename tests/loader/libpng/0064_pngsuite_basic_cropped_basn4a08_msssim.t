#!/bin/sh
# TAP test: PNGSuite case for basic/basn4a08.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/basic/basn4a08.png -colorspace RGB -background "#000" -alpha remove -alpha off -colorspace sRGB -crop 16x16+8+8 +repage -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0056_pngsuite_basic_cropped_basn4a08_msssim.ppm
set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn4a08.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0056_pngsuite_basic_cropped_basn4a08_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/basn4a08.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 -c16x16+8+8 -Llibpng:cms_engine=none! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.98" "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "basic_cropped basic/basn4a08.png"
exit 0
