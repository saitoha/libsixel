#!/bin/sh
# Verify MS-SSIM for clustering din99d working linear colorspace selection.
#
# Flow summary:
# - Convert the input image with -X din99d -W linear.
# - Compare the output against the original image.
# - Enforce MS-SSIM >= 0.98 via lsqa.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

lsqa_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/lsqa/lsqa_common.sh
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
export LSQA_HELPER_DIR
. "${lsqa_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0
lsqa_floor=0.98

ensure_img2sixel_available
echo "1..1"
set -v

input_image="${images_dir}/snake.png"
output_sixel="${artifact_dir}/cluster-din99d-work-linear.six"

require_file "${input_image}"

if run_img2sixel -t rgb -X din99d -W linear -o "${output_sixel}"     "${input_image}"     2>>"${log_file}"; then
    :
else
    fail 1 "img2sixel clustering din99d working linear conversion failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}"         "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "clustering din99d working linear lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "clustering din99d working linear lsqa failed"
fi

exit "${status}"
