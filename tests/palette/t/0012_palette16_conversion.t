#!/bin/sh
# Convert with a 16-colour palette using the fast encoder.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${images_dir}/snake.jpg"
map16_palette="${images_dir}/map16-palette.png"
target_sixel="${output_dir}/palette16.sixel"

require_file "${snake_jpg}"
require_file "${map16_palette}"

if run_img2sixel -7 -m "${map16_palette}" -Efast "${snake_jpg}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "16-colour palette conversion succeeds"
else
    fail 1 "16-colour palette conversion fails"
fi

exit "${status}"
