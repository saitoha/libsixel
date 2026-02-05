#!/bin/sh
# Verify MS-SSIM for clustering cielab working cielab colorspace selection.
#
# Flow summary:
# - Convert the input image with -X cielab -W cielab.
# - Compare the output against the original image.
# - Enforce MS-SSIM >= 0.98 via lsqa.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
. "${lsqa_common_path}"

setup_conversion_env "${test_name}"

status=0
lsqa_floor=0.98

ensure_img2sixel_available
echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/cluster-cielab-work-cielab.six"

if run_img2sixel -t rgb -X cielab -W cielab -o "${output_sixel}"     "${input_image}"; then
    :
else
    fail 1 "img2sixel clustering cielab working cielab conversion failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}"         "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "clustering cielab working cielab lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "clustering cielab working cielab lsqa failed"
fi

exit "${status}"
