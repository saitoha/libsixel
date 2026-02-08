#!/bin/sh
# TAP test: PNGSuite case for basic/basn3p02.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/basic/basn3p02.png -background "#000" -alpha remove -alpha off -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0027_pngsuite_basic_default_basn3p02_msssim.ppm
set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/loader/pngsuite_common.sh"

status=0

ensure_pngsuite_prereqs

echo "1..1"
set -v

input_png="${images_dir}/pngsuite/basic/basn3p02.png"
expected_ppm="${top_srcdir}/tests/data/loader/pngsuite_expected/0027_pngsuite_basic_default_basn3p02_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/basn3p02.sixel"
score_file="${ARTIFACT_LOCAL_DIR}/basn3p02.ms_ssim.txt"
img2sixel_opts=""

if [ ! -f "${expected_ppm}" ]; then
    fail 1 "missing expected image: 0027_pngsuite_basic_default_basn3p02_msssim.ppm"
    exit "${status}"
fi

if run_img2sixel ${img2sixel_opts} "${input_png}" >"${output_sixel}"; then
    if run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${expected_ppm}" - <"${output_sixel}" >"${score_file}"; then
        pass 1 "basic_default basic/basn3p02.png"
    else
        fail 1 "basic_default basic/basn3p02.png"
    fi
else
    fail 1 "basic_default basic/basn3p02.png"
fi

exit "${status}"
