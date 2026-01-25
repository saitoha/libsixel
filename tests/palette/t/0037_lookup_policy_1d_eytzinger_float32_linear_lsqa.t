#!/bin/sh
# Run lsqa checks for float32 1d-Eytzinger in the linear colorspace.
# Quality floors tuned to 99% of the current lsqa metrics:
# - MS-SSIM floor: 0.978230
# - PSNR_Y floor: 37.913774
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

palette_lsqa_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/lsqa/lsqa_palette_common.sh
PALETTE_LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${palette_lsqa_path}")" && pwd)
export PALETTE_LSQA_HELPER_DIR
. "${palette_lsqa_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available
ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

echo "1..1"

input_image="${images_dir}/snake.png"
output_sixel="${output_dir}/1d-eytzinger-float32-linear.six"
output_png="${output_dir}/1d-eytzinger-float32-linear.png"

require_file "${input_image}"

if ! palette_lsqa_init "$0"; then
    fail 1 "lsqa binary missing"
    exit "${status}"
fi

PALETTE_LSQA_MS_SSIM_FLOOR=0.978230
PALETTE_LSQA_PSNR_FLOOR=37.913774
export PALETTE_LSQA_MS_SSIM_FLOOR
export PALETTE_LSQA_PSNR_FLOOR

if run_img2sixel --lookup-policy=1d-eytzinger --precision=float32 \
        --working-colorspace=linear -d none \
        -o "${output_sixel}" "${input_image}" 2>>"${log_file}"; then
    :
else
    fail 1 "float32 1d-Eytzinger linear colorspace conversion failed"
    exit "${status}"
fi

if run_sixel2png -i "${output_sixel}" -o "${output_png}" \
        2>>"${log_file}"; then
    :
else
    fail 1 "sixel2png decode failed"
    exit "${status}"
fi

if palette_lsqa_assert_quality "${input_image}" "${output_png}" \
        "1d-eytzinger-float32-linear" "${artifact_dir}"; then
    pass 1 "float32 1d-Eytzinger linear colorspace lsqa passed"
else
    fail 1 "float32 1d-Eytzinger linear colorspace lsqa failed"
fi

exit "${status}"
