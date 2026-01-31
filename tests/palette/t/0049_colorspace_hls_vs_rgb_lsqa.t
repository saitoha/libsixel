#!/bin/sh
# Compare HLS and RGB conversions with MS-SSIM target.
#
# Flow summary:
# - Convert the input image with -t hls.
# - Convert the input image with -t rgb.
# - Compare both outputs and enforce MS-SSIM >= 0.99.
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
lsqa_floor=0.99

ensure_img2sixel_available
echo "1..1"
set -v

input_image="${images_dir}/snake.png"
output_hls="${artifact_dir}/hls.six"
output_rgb="${artifact_dir}/rgb.six"

require_file "${input_image}"


if run_img2sixel -t hls -o "${output_hls}" "${input_image}" \
    2>>"${log_file}"; then
    :
else
    fail 1 "img2sixel hls conversion failed"
    exit "${status}"
fi

if run_img2sixel -t rgb -o "${output_rgb}" "${input_image}" \
    2>>"${log_file}"; then
    :
else
    fail 1 "img2sixel rgb conversion failed"
    exit "${status}"
fi

lsqa_err=$(
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
