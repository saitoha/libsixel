#!/bin/sh
# Inspect grayscale PNG without errors.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_gray_png="${images_dir}/snake-grayscale.png"
target_txt="${ARTIFACT_LOCAL_DIR}/gray-png-inspection.txt"

if run_img2sixel -I "${snake_gray_png}" >"${target_txt}"; then
    pass 1 "grayscale PNG inspection succeeds"
else
    fail 1 "grayscale PNG inspection fails"
fi

exit "${status}"
