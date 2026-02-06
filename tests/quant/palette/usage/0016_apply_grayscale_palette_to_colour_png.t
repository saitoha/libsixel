#!/bin/sh
# Apply grayscale palette file to a colour PNG input.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

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
