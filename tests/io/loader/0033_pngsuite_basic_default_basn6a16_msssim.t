#!/bin/sh
# TAP test: PNGSuite case for basic/basn6a16.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/basic/basn6a16.png -background "#000" -alpha remove -alpha off -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0033_pngsuite_basic_default_basn6a16_msssim.ppm
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/loader/pngsuite_common.sh"

ensure_pngsuite_prereqs

echo "1..1"
set -v

input_png="${images_dir}/pngsuite/basic/basn6a16.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/0033_pngsuite_basic_default_basn6a16_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/basn6a16.sixel"
img2sixel_opts=""

if run_img2sixel ${img2sixel_opts} "${input_png}" >"${output_sixel}"; then
    if run_lsqa -m MS-SSIM -b "MS-SSIM:0.87" "${expected_ppm}" - <"${output_sixel}" >&2; then
        pass 1 "basic_default basic/basn6a16.png"
    else
        fail 1 "basic_default basic/basn6a16.png"
    fi
else
    fail 1 "basic_default basic/basn6a16.png"
fi

exit 0
