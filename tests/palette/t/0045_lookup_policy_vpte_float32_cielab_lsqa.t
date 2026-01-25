#!/bin/sh
# Run lsqa checks for float32 VPTE in the cielab colorspace.
# Quality floors tuned to requested QA thresholds:
# - MS-SSIM floor: 0.98
# - PSNR_Y floor: 34.3
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
output_sixel="${output_dir}/vpte-float32-cielab.six"
output_png="${output_dir}/vpte-float32-cielab.png"

require_file "${input_image}"

if ! palette_lsqa_init "$0"; then
    fail 1 "lsqa binary missing"
    exit "${status}"
fi

PALETTE_LSQA_MS_SSIM_FLOOR=0.98
PALETTE_LSQA_PSNR_FLOOR=34.3
export PALETTE_LSQA_MS_SSIM_FLOOR
export PALETTE_LSQA_PSNR_FLOOR

if run_img2sixel --lookup-policy=vpte --precision=float32 \
        --working-colorspace=cielab -d none \
        -o "${output_sixel}" "${input_image}" \
        2>>"${log_file}"; then
    :
else
    fail 1 "float32 VPTE cielab colorspace conversion failed"
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
        "vpte-float32-cielab" "${artifact_dir}"; then
    pass 1 "float32 VPTE cielab colorspace lsqa passed"
else
    fail 1 "float32 VPTE cielab colorspace lsqa failed"
fi

exit "${status}"
