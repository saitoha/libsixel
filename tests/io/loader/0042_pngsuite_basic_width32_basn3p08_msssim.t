#!/bin/sh
# TAP test: PNGSuite case for basic/basn3p08.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/basic/basn3p08.png -background "#000" -alpha remove -alpha off -resize 32x -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0042_pngsuite_basic_width32_basn3p08_msssim.ppm
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_feature_available "HAVE_LIBPNG" "png" "libpng support"
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

input_png="${images_dir}/pngsuite/basic/basn3p08.png"
expected_ppm="${top_srcdir}/tests/data/loader/pngsuite_expected/0042_pngsuite_basic_width32_basn3p08_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/basn3p08.sixel"
img2sixel_opts="-w32 -Llibpng!"

run_img2sixel ${img2sixel_opts} "${input_png}" >"${output_sixel}" || {
    fail 1 "img2sixel failed"
    exit 0
}

lsqa_msg=$(run_lsqa -m MS-SSIM -b "MS-SSIM:0.95" "${expected_ppm}" - <"${output_sixel}" 2>&1) || {
    fail 1 "$lsqa_msg"
    exit 0
}

pass 1 "basic_width32 basic/basn3p08.png"
exit 0
