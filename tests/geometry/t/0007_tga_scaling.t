#!/bin/sh
# Validate TGA conversion with scaling and palette options.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_tga="${images_dir}/snake.tga"
target_sixel="${tmp_dir}/snake-tga.sixel"

require_file "${snake_tga}"

if run_img2sixel -7 -sauto -w100 -rga -qauto -dburkes -tauto \
    "${snake_tga}" >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "TGA conversion with scaling succeeded"
else
    fail 1 "TGA conversion with scaling failed"
fi

exit "${status}"
