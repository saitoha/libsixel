#!/bin/sh
# TAP test: PNGSuite case for basic/basn0g04.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/basic/basn0g04.png -background "#000" -alpha remove -alpha off -resize 32x -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0036_pngsuite_basic_width32_basn0g04_msssim.ppm
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_feature_available "HAVE_LIBPNG" "png" "libpng support"
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

input_png="${images_dir}/pngsuite/basic/basn0g04.png"
expected_ppm="${top_srcdir}/tests/data/loader/pngsuite_expected/0036_pngsuite_basic_width32_basn0g04_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/basn0g04.sixel"
img2sixel_opts="-w32"

run_img2sixel ${img2sixel_opts} "${input_png}" >"${output_sixel}" || {
    fail 1 "img2sixel failed"
    exit 0
}

run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${expected_ppm}" - <"${output_sixel}" >&2 || {
    fail 1 "basic_width32 basic/basn0g04.png"
    exit 0
}

pass 1 "basic_width32 basic/basn0g04.png"
exit 0
