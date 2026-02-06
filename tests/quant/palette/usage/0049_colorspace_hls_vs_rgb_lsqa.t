#!/bin/sh
# Compare HLS and RGB conversions with MS-SSIM target.
#
# Flow summary:
# - Convert the input image with -t hls.
# - Convert the input image with -t rgb.
# - Compare both outputs and enforce MS-SSIM >= 0.99.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"

status=0
lsqa_floor=0.99

config_macro_defined HAVE_IMG2SIXEL || skip_all
echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_hls="${ARTIFACT_LOCAL_DIR}/hls.six"
output_rgb="${ARTIFACT_LOCAL_DIR}/rgb.six"

run_img2sixel -t hls -o "${output_hls}" "${input_image}" || {
    fail 1 "img2sixel hls conversion failed"
    exit "${status}"
}

run_img2sixel -t rgb -o "${output_rgb}" "${input_image}" || {
    fail 1 "img2sixel rgb conversion failed"
    exit "${status}"
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${output_rgb}" "${output_hls}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "hls vs rgb lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "hls vs rgb lsqa failed"
fi

exit "${status}"
