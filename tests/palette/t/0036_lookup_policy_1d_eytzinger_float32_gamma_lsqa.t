#!/bin/sh
# Run lsqa checks for float32 Eytzinger in the gamma colorspace.
# Quality floors tuned to 99% of the current lsqa MS-SSIM metric:
# - MS-SSIM floor: 0.976111
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

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.6}

ensure_img2sixel_available
ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

echo "1..1"

input_image="${images_dir}/snake.png"
output_sixel="${output_dir}/eytzinger-float32-gamma.six"
output_png="${output_dir}/eytzinger-float32-gamma.png"

require_file "${input_image}"

if ! lsqa_init "$0"; then
    fail 1 "lsqa binary missing"
    exit "${status}"
fi

LSQA_MS_SSIM_FLOOR=0.976111
export LSQA_MS_SSIM_FLOOR

if run_img2sixel --lookup-policy=eytzinger --precision=float32 \
        --working-colorspace=gamma -d none \
        -o "${output_sixel}" "${input_image}" 2>>"${log_file}"; then
    :
else
    fail 1 "float32 Eytzinger gamma colorspace conversion failed"
    exit "${status}"
fi

if run_sixel2png -i "${output_sixel}" -o "${output_png}" \
        2>>"${log_file}"; then
    :
else
    fail 1 "sixel2png decode failed"
    exit "${status}"
fi

if lsqa_assert_quality "${input_image}" "${output_png}" \
        "eytzinger-float32-gamma" "${artifact_dir}" "${lsqa_floor}"; then
    pass 1 "float32 Eytzinger gamma colorspace lsqa passed"
else
    fail 1 "float32 Eytzinger gamma colorspace lsqa failed"
fi

exit "${status}"
