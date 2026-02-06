#!/bin/sh
# Ensure 8-bit grayscale output succeeds.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_tga="${images_dir}/snake.tga"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray8.sixel"

if run_img2sixel -bgray8 -w120 "${snake_tga}" >"${target_sixel}"; then
    pass 1 "8-bit grayscale output succeeds"
else
    fail 1 "8-bit grayscale output fails"
fi

exit "${status}"
