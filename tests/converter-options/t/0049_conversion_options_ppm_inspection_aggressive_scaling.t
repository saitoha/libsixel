#!/bin/sh
# Inspect PPM while applying aggressive scaling and filters.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_ppm="${images_dir}/snake.ppm"
target_txt="${output_dir}/ppm-inspection.txt"

require_file "${snake_ppm}"

if run_img2sixel -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs \
        -rbilinear -trgb "${snake_ppm}" >"${target_txt}" 2>>"${log_file}"; then
    pass 1 "PPM inspection tolerates aggressive scaling"
else
    fail 1 "PPM inspection with scaling fails"
fi

exit "${status}"
