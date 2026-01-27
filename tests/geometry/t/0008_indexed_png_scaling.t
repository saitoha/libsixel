#!/bin/sh
# Ensure indexed PNG scales to a larger width.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

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
