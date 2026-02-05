#!/bin/sh
# Inspect PPM while applying aggressive scaling and filters.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_ppm="${images_dir}/snake.ppm"
target_txt="${output_dir}/ppm-inspection.txt"



if run_img2sixel -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs \
        -rbilinear -trgb "${snake_ppm}" >"${target_txt}"; then
    pass 1 "PPM inspection tolerates aggressive scaling"
else
    fail 1 "PPM inspection with scaling fails"
fi

exit "${status}"
