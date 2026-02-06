#!/bin/sh
# Validate TGA conversion with scaling and palette options.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_tga="${images_dir}/snake.tga"
target_sixel="${ARTIFACT_LOCAL_DIR}/snake-tga.sixel"

if run_img2sixel -7 -sauto -w100 -rga -qauto -dburkes -tauto \
    "${snake_tga}" >"${target_sixel}"; then
    pass 1 "TGA conversion with scaling succeeded"
else
    fail 1 "TGA conversion with scaling failed"
fi

exit "${status}"
