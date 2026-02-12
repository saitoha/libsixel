#!/bin/sh
# Convert PPM with a 16-colour palette and scaling.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_ppm="${top_srcdir}/tests/data/inputs/snake_64.ppm"
map16_png="${images_dir}/map16.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/ppm-palette16.sixel"

run_img2sixel -m "${map16_png}" -w100 -hauto -rbicubic -dauto "${snake_ppm}" >"${target_sixel}" || {
    fail 1 "PPM conversion with 16-colour palette fails"
    exit 0
}

pass 1 "PPM conversion with 16-colour palette works"

exit 0
