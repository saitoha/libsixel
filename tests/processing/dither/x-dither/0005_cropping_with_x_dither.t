#!/bin/sh
# Crop grayscale PNG using X ordered dither.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_gray_png="${images_dir}/snake-grayscale.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/crop-x-dither.sixel"

run_img2sixel -c200x200+100+100 -dx_dither "${snake_gray_png}" >"${target_sixel}" || {
    fail 1 "cropping with X ordered dither fails"
    exit "${status}"
}

pass 1 "cropping with X ordered dither succeeds"
exit "${status}"
