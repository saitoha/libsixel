#!/bin/sh
# TAP test: PNGSuite case for background/bggn4a16.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/background/bggn4a16.png -background "green" -alpha remove -alpha off -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0065_pngsuite_background_default_bggn4a16_msssim.ppm
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_feature_available "HAVE_LIBPNG" "png" "libpng support"
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/images/pngsuite/background/bggn4a16.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0065_pngsuite_background_default_bggn4a16_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/bggn4a16.sixel"

run_img2sixel -Llibpng! "${input_png}" >"${output_sixel}" || {
    fail 1 "img2sixel failed"
    exit 0
}

lsqa_msg=$(run_lsqa -m MS-SSIM -b "MS-SSIM:0.84" "${expected_ppm}" "${output_sixel}" 2>&1) || {
    fail 1 "$lsqa_msg"
    exit 0
}

pass 1 "background_default background/bggn4a16.png"
exit 0
