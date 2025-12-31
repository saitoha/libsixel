#!/bin/sh
# Validate fast encoder when using an external palette.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_ppm="${images_dir}/snake.ppm"
map8_palette="${images_dir}/map8-palette.png"

require_file "${snake_ppm}"
require_file "${map8_palette}"

if run_img2sixel -8 -m "${map8_palette}" -Esize "${snake_ppm}" \
        -o/dev/null 2>>"${log_file}"; then
    pass 1 "fast encoder with palette succeeds"
else
    fail 1 "fast encoder with palette fails"
fi

exit "${status}"
