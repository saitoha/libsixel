#!/bin/sh
# Verify MS-SSIM for clustering linear working linear colorspace selection.
#
# Flow summary:
# - Convert the input image with -X linear -W linear.
# - Compare the output against the original image.
# - Enforce MS-SSIM >= 0.98 via lsqa.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"

status=0
lsqa_floor=0.98

config_macro_defined HAVE_IMG2SIXEL || skip_all
echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/cluster-linear-work-linear.six"

run_img2sixel -t rgb -X linear -W linear -o "${output_sixel}" "${input_image}" || {
    fail 1 "img2sixel clustering linear working linear conversion failed"
    exit "${status}"
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}"         "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "clustering linear working linear lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "clustering linear working linear lsqa failed"
fi

exit "${status}"
