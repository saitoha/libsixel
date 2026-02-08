#!/bin/sh
# TAP test: PNGSuite case for background/bgai4a08.png with direct LSQA comparison.

# Reference image generation command:
#   magick images/pngsuite/background/bgai4a08.png -background "#fff" -alpha remove -alpha off -depth 8 \
#       -define ppm:format=plain PPM:tests/data/loader/pngsuite_expected/0068_pngsuite_background_white_bgai4a08_msssim.ppm
set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/loader/pngsuite_common.sh"

status=0

ensure_pngsuite_prereqs

echo "1..1"
set -v

input_png="${images_dir}/pngsuite/background/bgai4a08.png"
expected_ppm="${top_srcdir}/tests/data/loader/pngsuite_expected/0068_pngsuite_background_white_bgai4a08_msssim.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/bgai4a08.sixel"
score_file="${ARTIFACT_LOCAL_DIR}/bgai4a08.ms_ssim.txt"
img2sixel_opts="-B#fff"

if [ ! -f "${expected_ppm}" ]; then
    fail 1 "missing expected image: 0068_pngsuite_background_white_bgai4a08_msssim.ppm"
    exit "${status}"
fi

if run_img2sixel ${img2sixel_opts} "${input_png}" >"${output_sixel}"; then
    if run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${expected_ppm}" - <"${output_sixel}" >"${score_file}"; then
        pass 1 "background_white background/bgai4a08.png"
    else
        fail 1 "background_white background/bgai4a08.png"
    fi
else
    fail 1 "background_white background/bgai4a08.png"
fi

exit "${status}"
