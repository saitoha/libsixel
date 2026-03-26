#!/bin/sh
# TAP test: PNGSuite case for background/bgwn6a08.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/background/bgwn6a08.png -colorspace RGB -background "white" -alpha remove -alpha off -colorspace sRGB -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0066_pngsuite_background_default_bgwn6a08_msssim.ppm
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

input_png="${TOP_SRCDIR}/images/pngsuite/background/bgwn6a08.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0066_pngsuite_background_default_bgwn6a08_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/bgwn6a08.sixel"

run_img2sixel --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 -Llibpng:cms_engine=none! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "$lsqa_msg"
    exit 0
}

echo "ok" 1 - "background_default background/bgwn6a08.png"
exit 0
