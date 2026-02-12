#!/bin/sh
# Compare HLS and RGB conversions with MS-SSIM target.
#
# Flow summary:
# - Convert the input image with -t hls.
# - Convert the input image with -t rgb.
# - Compare both outputs and enforce MS-SSIM >= 0.99.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

lsqa_floor=0.99

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_hls="${ARTIFACT_LOCAL_DIR}/hls.six"
output_rgb="${ARTIFACT_LOCAL_DIR}/rgb.six"

run_img2sixel -t hls -o "${output_hls}" "${input_image}" || {
    fail 1 "img2sixel hls conversion failed"
    exit 0
}

run_img2sixel -t rgb -o "${output_rgb}" "${input_image}" || {
    fail 1 "img2sixel rgb conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${output_rgb}" "${output_hls}" 2>&1
) || lsqa_run_status=$?

[ "${lsqa_run_status:-0}" -eq 0 ] && {
    pass 1 "hls vs rgb lsqa passed"
    exit 0
}

[ "${lsqa_run_status}" -eq 5 ] && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "hls vs rgb lsqa failed"

exit 0
