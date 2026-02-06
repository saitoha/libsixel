#!/bin/sh
# Convert JPEG using external palette and Welsh filter.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

egret_jpg="${images_dir}/egret.jpg"
map8_png="${images_dir}/map8.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/jpeg-welsh.sixel"

if run_img2sixel -m "${map8_png}" -w200 -fau -rwelsh "${egret_jpg}" >"${target_sixel}"; then
    pass 1 "JPEG conversion using palette and Welsh filter"
else
    fail 1 "JPEG palette Welsh conversion fails"
fi

exit "${status}"
