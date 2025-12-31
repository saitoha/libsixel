#!/bin/sh
# Encode existing Sixel data directly.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_six="${images_dir}/snake.six"
target_sixel="${output_dir}/sixel-direct.sixel"

require_file "${snake_six}"

if run_img2sixel -e "${snake_six}" >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "direct Sixel encode emits data"
else
    fail 1 "direct Sixel encode failed"
fi

exit "${status}"
