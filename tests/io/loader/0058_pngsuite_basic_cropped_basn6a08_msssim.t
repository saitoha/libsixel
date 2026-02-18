#!/bin/sh
# TAP test: PNGSuite case for basic/basn6a08.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/basic/basn6a08.png -background "#000" -alpha remove -alpha off -crop 16x16+8+8 +repage -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0058_pngsuite_basic_cropped_basn6a08_msssim.ppm
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn6a08.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0058_pngsuite_basic_cropped_basn6a08_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/basn6a08.sixel"

run_img2sixel -c16x16+8+8 -Llibpng! "${input_png}" >"${output_sixel}" || {
    fail 1 "img2sixel failed"
    exit 0
}

lsqa_msg=$(run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${expected_ppm}" "${output_sixel}" 2>&1) || {
    fail 1 "$lsqa_msg"
    exit 0
}

pass 1 "basic_cropped basic/basn6a08.png"
exit 0
