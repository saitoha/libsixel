#!/bin/sh
# Verify long option forms are accepted.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_jpg="${images_dir}/snake.jpg"
longopt_sixel="${tmp_dir}/snake-longopt.sixel"

require_file "${snake_jpg}"

if run_img2sixel --height=100 --diffusion=atkinson \
    --outfile="${longopt_sixel}" <"${snake_jpg}" 2>>"${log_file}"; then
    pass 1 "long option forms accepted"
else
    fail 1 "long option forms failed"
fi

exit "${status}"
