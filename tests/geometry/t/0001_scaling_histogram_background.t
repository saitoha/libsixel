#!/bin/sh
# Validate scaling with histogram selection and background colour.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_jpg="${images_dir}/snake.jpg"
snake_scaling="${tmp_dir}/snake-scaling.sixel"

require_file "${snake_jpg}"

if run_img2sixel -w50% -h150% -dfs -Bblue -thls -shist <"${snake_jpg}" \
    | tee "${snake_scaling}" >/dev/null 2>>"${log_file}"; then
    pass 1 "scaling with histogram and background succeeded"
else
    fail 1 "scaling with histogram and background failed"
fi

exit "${status}"
