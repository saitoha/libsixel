#!/bin/sh
# Convert grayscale PNG without palette overrides.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_gray_png="${images_dir}/snake-grayscale.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray-png.sixel"

if run_img2sixel "${snake_gray_png}" >"${target_sixel}"; then
    pass 1 "grayscale PNG conversion succeeds"
else
    fail 1 "grayscale PNG conversion fails"
fi

exit "${status}"
