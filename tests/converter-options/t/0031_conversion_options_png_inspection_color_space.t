#!/bin/sh
# Verify PNG inspection sets colour space in the report.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_png="${images_dir}/snake.png"
target_txt="${output_dir}/png-inspection.txt"

require_file "${snake_png}"

if run_img2sixel -I -C10 -djajuni "${snake_png}" \
        >"${target_txt}" 2>>"${log_file}"; then
    pass 1 "PNG inspection sets colour space"
else
    fail 1 "PNG inspection colour space failed"
fi

exit "${status}"
