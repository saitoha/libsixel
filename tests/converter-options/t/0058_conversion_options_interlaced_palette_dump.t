#!/bin/sh
# Emit palette dump while performing interlaced encode.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_jpg="${images_dir}/snake.jpg"
target_sixel="${output_dir}/interlaced-palette-dump.sixel"

require_file "${snake_jpg}"

if run_img2sixel -e -i -P "${snake_jpg}" >"${target_sixel}" \
        2>>"${log_file}"; then
    pass 1 "interlaced encode emits palette dump"
else
    fail 1 "interlaced encode palette dump fails"
fi

exit "${status}"
