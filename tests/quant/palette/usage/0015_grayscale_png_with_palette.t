#!/bin/sh
# Apply external palette to grayscale PNG conversion.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_gray_png="${images_dir}/snake-grayscale.png"
map8_palette="${images_dir}/map8-palette.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray-png-palette.sixel"

if run_img2sixel -m "${map8_palette}" "${snake_gray_png}" >"${target_sixel}"; then
    pass 1 "grayscale PNG with external palette works"
else
    fail 1 "grayscale PNG palette conversion fails"
fi

exit "${status}"
