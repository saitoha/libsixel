#!/bin/sh
# Inspect BMP while applying palette and scaling filters.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_bmp="${images_dir}/snake.bmp"
target_txt="${output_dir}/bmp-inspection.txt"

require_file "${snake_bmp}"

if run_img2sixel -I -v -w200 -hauto -c100x1000+40+20 -qlow -dnone \
        -rhamming -thls "${snake_bmp}" >"${target_txt}" 2>>"${log_file}"; then
    pass 1 "BMP inspection with filters succeeds"
else
    fail 1 "BMP inspection with filters fails"
fi

exit "${status}"
