#!/bin/sh
# TAP test: PNGSuite case for basic/basn4a16.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/basic/basn4a16.png -colorspace RGB -background "#000" -alpha remove -alpha off -colorspace sRGB -resize 32x -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0044_pngsuite_basic_width32_basn4a16_msssim.ppm
set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn4a16.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0044_pngsuite_basic_width32_basn4a16_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/basn4a16.sixel"

run_img2sixel -w32 -Llibpng:enable_cms=0! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "basic_width32 basic/basn4a16.png"
exit 0
