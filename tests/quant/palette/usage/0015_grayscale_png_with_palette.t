#!/bin/sh
# Apply external palette to grayscale PNG conversion.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_gray_png="${images_dir}/snake-grayscale.png"
map8_palette="${images_dir}/map8-palette.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray-png-palette.sixel"

run_img2sixel -m "${map8_palette}" "${snake_gray_png}" >"${target_sixel}" || {
    fail 1 "grayscale PNG palette conversion fails"
    exit 0
}

pass 1 "grayscale PNG with external palette works"

exit 0
