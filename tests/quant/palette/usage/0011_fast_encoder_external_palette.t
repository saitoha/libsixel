#!/bin/sh
# Validate fast encoder when using an external palette.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_ppm="${top_srcdir}/tests/data/inputs/snake_64.ppm"
map8_palette="${images_dir}/map8-palette.png"

if run_img2sixel -8 -m "${map8_palette}" -Esize "${snake_ppm}" \
        -o/dev/null; then
    pass 1 "fast encoder with palette succeeds"
else
    fail 1 "fast encoder with palette fails"
fi

exit "${status}"
