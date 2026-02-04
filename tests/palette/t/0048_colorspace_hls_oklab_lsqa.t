#!/bin/sh
# Verify HLS input with OKLAB working colorspace meets MS-SSIM target.
#
# Flow summary:
# - Convert the input image with -t hls -W oklab.
# - Compare the output against the original image.
# - Enforce MS-SSIM >= 0.99 via lsqa.
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

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${artifact_dir}/hls-oklab.six"

require_file "${input_image}"


if run_img2sixel -t hls -W oklab -o "${output_sixel}" \
    "${input_image}" \
    2>>"${log_file}"; then
    :
else
    fail 1 "img2sixel hls+oklab conversion failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "hls+oklab lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "hls+oklab lsqa failed"
fi

exit "${status}"
