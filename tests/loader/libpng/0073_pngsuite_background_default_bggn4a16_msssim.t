#!/bin/sh
# TAP test: PNGSuite case for background/bggn4a16.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/background/bggn4a16.png -colorspace RGB \
#       -background '#ababab' -alpha remove -alpha off -colorspace sRGB -depth 8 \
#       -define ppm:format=plain \
#       PPM:tests/data/loader/pngsuite_expected/0065_pngsuite_background_default_bggn4a16_msssim.ppm
#
# Why '#ababab'?
# - bggn4a16.png is a GA16 PNG that carries gAMA and bKGD chunks.
# - The file bKGD sample is 0xAB84 (16-bit gray), which maps to about 0xAB in 8-bit.
# - '#ababab' tracks libpng's current default composition result while keeping this
#   test independent from img2sixel output and preserving a strict quality gate
#   (MS-SSIM >= 0.98).
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

input_png="${TOP_SRCDIR}/images/pngsuite/background/bggn4a16.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0065_pngsuite_background_default_bggn4a16_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/bggn4a16.sixel"

run_img2sixel -Llibpng:cms_engine=none! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "background_default background/bggn4a16.png"
exit 0
