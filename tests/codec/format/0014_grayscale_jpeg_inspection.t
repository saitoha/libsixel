#!/bin/sh
# Inspect grayscale JPEG without errors.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_gray_jpg="${images_dir}/snake-grayscale.jpg"
target_txt="${ARTIFACT_LOCAL_DIR}/gray-jpeg-inspection.txt"

run_img2sixel -I "${snake_gray_jpg}" >"${target_txt}" || {
    fail 1 "grayscale JPEG inspection fails"
    exit 0
}

pass 1 "grayscale JPEG inspection succeeds"

exit 0
