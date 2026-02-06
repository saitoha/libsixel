#!/bin/sh
# Inspect grayscale JPEG without errors.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_gray_jpg="${images_dir}/snake-grayscale.jpg"
target_txt="${ARTIFACT_LOCAL_DIR}/gray-jpeg-inspection.txt"

if run_img2sixel -I "${snake_gray_jpg}" >"${target_txt}"; then
    pass 1 "grayscale JPEG inspection succeeds"
else
    fail 1 "grayscale JPEG inspection fails"
fi

exit "${status}"
