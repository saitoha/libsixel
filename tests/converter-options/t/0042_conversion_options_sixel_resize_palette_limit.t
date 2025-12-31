#!/bin/sh
# Resize Sixel input while constraining palette size.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_six="${images_dir}/snake.six"
target_sixel="${output_dir}/sixel-resize.sixel"

require_file "${snake_six}"

if run_img2sixel -w200 -p8 "${snake_six}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "Sixel resizing with palette limit succeeds"
else
    fail 1 "Sixel resizing with palette limit fails"
fi

exit "${status}"
