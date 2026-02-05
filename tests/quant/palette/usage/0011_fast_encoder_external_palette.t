#!/bin/sh
# Validate fast encoder when using an external palette.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

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
