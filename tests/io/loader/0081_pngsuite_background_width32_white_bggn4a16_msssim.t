#!/bin/sh
# TAP test: PNGSuite case for background/bggn4a16.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/background/bggn4a16.png -background "#fff" -alpha remove -alpha off -resize 32x -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0081_pngsuite_background_width32_white_bggn4a16_msssim.ppm
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_feature_available "HAVE_LIBPNG" "png" "libpng support"
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

input_png="${images_dir}/pngsuite/background/bggn4a16.png"
expected_ppm="${top_srcdir}/tests/data/loader/pngsuite_expected/0081_pngsuite_background_width32_white_bggn4a16_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/bggn4a16.sixel"

run_img2sixel -w32 -B#fff -Llibpng! "${input_png}" >"${output_sixel}" || {
    fail 1 "img2sixel failed"
    exit 0
}

run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${expected_ppm}" - <"${output_sixel}" >&2 || {
    fail 1 "LSQA failed"
    exit 0
}

pass 1 "background_width32_white background/bggn4a16.png"
exit 0
