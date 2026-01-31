#!/bin/sh
# Verify RGB input with CIELAB working colorspace meets MS-SSIM target.
#
# Flow summary:
# - Convert the input image with -t rgb -W cielab.
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
lsqa_floor=0.99

ensure_img2sixel_available
echo "1..1"
set -v

input_image="${images_dir}/snake.png"
output_sixel="${artifact_dir}/rgb-cielab.six"

require_file "${input_image}"

if ! lsqa_init "$0"; then
    fail 1 "lsqa binary missing"
    exit "${status}"
fi

if run_img2sixel -t rgb -W cielab -o "${output_sixel}" \
        "${input_image}" \
        2>>"${log_file}"; then
    :
else
    fail 1 "img2sixel rgb+cielab conversion failed"
    exit "${status}"
fi

if lsqa_run_benchmark "${input_image}" "${output_sixel}" \
        "rgb-cielab" "${artifact_dir}" "${lsqa_floor}"; then
    pass 1 "rgb+cielab lsqa passed"
else
    fail 1 "rgb+cielab lsqa failed"
fi

exit "${status}"
