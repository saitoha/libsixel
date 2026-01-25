#!/bin/sh
# Run lsqa checks for float32 1d-Eytzinger in the CIELAB colorspace.
# Quality floors tuned for CIELAB output:
# - MS-SSIM floor: 0.58
# - PSNR_Y floor: 19.5
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
output_sixel="${output_dir}/1d-eytzinger-float32-cielab.six"
output_png="${output_dir}/1d-eytzinger-float32-cielab.png"

require_file "${input_image}"

if ! palette_lsqa_init "$0"; then
    fail 1 "lsqa binary missing"
    exit "${status}"
fi

PALETTE_LSQA_MS_SSIM_FLOOR=0.58
PALETTE_LSQA_PSNR_FLOOR=19.5
export PALETTE_LSQA_MS_SSIM_FLOOR
export PALETTE_LSQA_PSNR_FLOOR

if run_img2sixel --lookup-policy=1d-eytzinger --precision=float32 \
        --working-colorspace=cielab -p 16 -d none \
        -o "${output_sixel}" "${input_image}" 2>>"${log_file}"; then
    :
else
    fail 1 "float32 1d-Eytzinger CIELAB colorspace conversion failed"
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
        "1d-eytzinger-float32-cielab" "${artifact_dir}"; then
    pass 1 "float32 1d-Eytzinger CIELAB colorspace lsqa passed"
else
    fail 1 "float32 1d-Eytzinger CIELAB colorspace lsqa failed"
fi

exit "${status}"
