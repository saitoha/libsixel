#!/bin/sh
# Convert with a 16-colour palette using the fast encoder.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
map16_palette="${TOP_SRCDIR}/images/map16-palette.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/palette16.sixel"

run_img2sixel -7 -m "${map16_palette}" -Efast "${snake_jpg}" >"${target_sixel}" || {
    fail 1 "16-colour palette conversion fails"
    exit 0
}

pass 1 "16-colour palette conversion succeeds"

exit 0
