#!/bin/sh
# Validate scaling with histogram selection and background colour.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${images_dir}/snake.jpg"
snake_scaling="${tmp_dir}/snake-scaling.sixel"



if run_img2sixel -w50% -h150% -dfs -Bblue -thls -shist <"${snake_jpg}" \
    | tee "${snake_scaling}" >/dev/null; then
    pass 1 "scaling with histogram and background succeeded"
else
    fail 1 "scaling with histogram and background failed"
fi

exit "${status}"
