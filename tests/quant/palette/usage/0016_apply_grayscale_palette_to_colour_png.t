#!/bin/sh
# Apply grayscale palette file to a colour PNG input.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_gray_png="${images_dir}/snake-grayscale.png"
snake_png="${top_srcdir}/tests/data/inputs/snake_64.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray-palette-colour.sixel"

if run_img2sixel -m "${snake_gray_png}" "${snake_png}" >"${target_sixel}"; then
    pass 1 "grayscale palette applied to colour PNG"
else
    fail 1 "grayscale palette application fails"
fi

exit "${status}"
