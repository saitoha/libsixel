#!/bin/sh
# Verify img2sixel roundtrips preserve image quality (SSIM/PSNR) against the
# original PNG input.

set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../conversion/common.sh
. "${conversion_common_path}"

quality_helper="${top_srcdir}/tests/common/quality.sh"
. "${quality_helper}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available
ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

quality_require_lsqa

echo "1..1"

input_png="${top_srcdir}/tests/data/inputs/formats/rgb.png"
require_file "${input_png}"

sixel_out="${output_dir}/rgb.six"
png_out="${output_dir}/rgb.png"

if run_img2sixel -o "${sixel_out}" "${input_png}" 2>>"${log_file}" \
        && run_sixel2png -o "${png_out}" "${sixel_out}" \
        && quality_compare_images "${input_png}" "${png_out}" 0.98 32 \
            "${log_file}"; then
    pass 1 "img2sixel roundtrip matches baseline PNG (SSIM/PSNR)"
else
    fail 1 "img2sixel roundtrip diverged from baseline PNG"
fi

exit "${status}"
