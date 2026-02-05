#!/bin/sh
# Convert with a 16-colour palette using the fast encoder.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${top_srcdir}/tests/data/inputs/snake_64.jpg"
map16_palette="${images_dir}/map16-palette.png"
target_sixel="${output_dir}/palette16.sixel"




if run_img2sixel -7 -m "${map16_palette}" -Efast "${snake_jpg}" \
        >"${target_sixel}"; then
    pass 1 "16-colour palette conversion succeeds"
else
    fail 1 "16-colour palette conversion fails"
fi

exit "${status}"
