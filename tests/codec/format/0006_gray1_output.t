#!/bin/sh
# Ensure 1-bit grayscale output succeeds.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_tga="${TOP_SRCDIR}/tests/data/inputs/snake_64.tga"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray1.sixel"

run_img2sixel -bgray1 -w120 "${snake_tga}" >"${target_sixel}" || {
    fail 1 "1-bit grayscale output fails"
    exit 0
}

pass 1 "1-bit grayscale output succeeds"

exit 0
