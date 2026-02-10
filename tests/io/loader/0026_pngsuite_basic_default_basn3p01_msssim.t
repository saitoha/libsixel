#!/bin/sh
# TAP test: PNGSuite case for basic/basn3p01.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/basic/basn3p01.png -background "#000" -alpha remove -alpha off -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0026_pngsuite_basic_default_basn3p01_msssim.ppm
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/loader/pngsuite_common.sh"

ensure_pngsuite_prereqs

echo "1..1"
set -v

input_png="${images_dir}/pngsuite/basic/basn3p01.png"
expected_ppm="${top_srcdir}/tests/data/loader/pngsuite_expected/0026_pngsuite_basic_default_basn3p01_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/basn3p01.sixel"
score_file="${ARTIFACT_LOCAL_DIR}/basn3p01.ms_ssim.txt"
img2sixel_opts=""

if [ ! -f "${expected_ppm}" ]; then
    fail 1 "missing expected image: 0026_pngsuite_basic_default_basn3p01_msssim.ppm"
    exit 0
fi

if run_img2sixel ${img2sixel_opts} "${input_png}" >"${output_sixel}"; then
    if run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${expected_ppm}" - <"${output_sixel}" >"${score_file}"; then
        pass 1 "basic_default basic/basn3p01.png"
    else
        fail 1 "basic_default basic/basn3p01.png"
    fi
else
    fail 1 "basic_default basic/basn3p01.png"
fi

exit 0
