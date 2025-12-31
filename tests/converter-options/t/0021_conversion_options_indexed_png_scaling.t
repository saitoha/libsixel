#!/bin/sh
# Ensure indexed PNG scales to a larger width.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_palette_png="${images_dir}/snake-palette.png"
target_sixel="${output_dir}/indexed-scale.sixel"

require_file "${snake_palette_png}"

if run_img2sixel -7 -w300 "${snake_palette_png}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "indexed PNG scales to large width"
else
    fail 1 "indexed PNG scaling fails"
fi

exit "${status}"
