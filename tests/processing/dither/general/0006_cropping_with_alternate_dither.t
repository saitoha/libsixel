#!/bin/sh
# Crop grayscale PNG using alternate ordered dither.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_gray_png="${images_dir}/snake-grayscale.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/crop-alt-dither.sixel"

if run_img2sixel -c200x200+100+100 -w400 -da_dither \
        "${snake_gray_png}" >"${target_sixel}"; then
    pass 1 "cropping with alternate dither succeeds"
else
    fail 1 "cropping with alternate dither fails"
fi

exit "${status}"
