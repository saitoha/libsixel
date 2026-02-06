#!/bin/sh
# Verify MS-SSIM for clustering cielab working linear colorspace selection.
#
# Flow summary:
# - Convert the input image with -X cielab -W linear.
# - Compare the output against the original image.
# - Enforce MS-SSIM >= 0.98 via lsqa.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

lsqa_floor=0.98

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"
echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/cluster-cielab-work-linear.six"

run_img2sixel -t rgb -X cielab -W linear -o "${output_sixel}" "${input_image}" || {
    fail 1 "img2sixel clustering cielab working linear conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}"         "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "clustering cielab working linear lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "clustering cielab working linear lsqa failed"
fi

exit 0
