#!/bin/sh
# TAP test: PNGSuite case for background/bggn4a16.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/background/bggn4a16.png -background "#fff" -alpha remove -alpha off -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0073_pngsuite_background_white_bggn4a16_msssim.ppm
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

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

input_png="${TOP_SRCDIR}/images/pngsuite/background/bggn4a16.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0073_pngsuite_background_white_bggn4a16_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/bggn4a16.sixel"

run_img2sixel -B#fff -Llibpng! "${input_png}" >"${output_sixel}" || {
    fail 1 "img2sixel failed"
    exit 0
}

lsqa_msg=$(run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${expected_ppm}" "${output_sixel}" 2>&1) || {
    fail 1 "$lsqa_msg"
    exit 0
}

pass 1 "background_white background/bggn4a16.png"
exit 0
