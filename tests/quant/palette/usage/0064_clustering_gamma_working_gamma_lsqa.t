#!/bin/sh
# Verify MS-SSIM for clustering gamma working gamma colorspace selection.
#
# Flow summary:
# - Convert the input image with -X gamma -W gamma.
# - Compare the output against the original image.
# - Enforce MS-SSIM >= 0.98 via lsqa.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

lsqa_floor=0.98

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"
echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/cluster-gamma-work-gamma.six"

run_img2sixel -t rgb -X gamma -W gamma -o "${output_sixel}" "${input_image}" || {
    fail 1 "img2sixel clustering gamma working gamma conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}"         "${output_sixel}" 2>&1
) || lsqa_run_status=$?

[ "${lsqa_run_status:-0}" -eq 0 ] && {
    pass 1 "clustering gamma working gamma lsqa passed"
    exit 0
}

[ "${lsqa_run_status}" -eq 5 ] && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "clustering gamma working gamma lsqa failed"

exit 0
