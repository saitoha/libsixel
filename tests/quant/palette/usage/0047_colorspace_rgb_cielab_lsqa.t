#!/bin/sh
# Verify RGB input with CIELAB working colorspace meets MS-SSIM target.
#
# Flow summary:
# - Convert the input image with -t rgb -W cielab.
# - Compare the output against the original image.
# - Enforce MS-SSIM >= 0.99 via lsqa.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"

status=0
lsqa_floor=0.99

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"
echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/rgb-cielab.six"

run_img2sixel -t rgb -W cielab -o "${output_sixel}" "${input_image}" || {
    fail 1 "img2sixel rgb+cielab conversion failed"
    exit "${status}"
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "rgb+cielab lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "rgb+cielab lsqa failed"
fi

exit "${status}"
