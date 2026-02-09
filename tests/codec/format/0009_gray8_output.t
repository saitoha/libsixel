#!/bin/sh
# Ensure 8-bit grayscale output succeeds.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_tga="${top_srcdir}/tests/data/inputs/snake_64.tga"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray8.sixel"

if run_img2sixel -bgray8 -w120 "${snake_tga}" >"${target_sixel}"; then
    pass 1 "8-bit grayscale output succeeds"
else
    fail 1 "8-bit grayscale output fails"
fi

exit "${status}"
