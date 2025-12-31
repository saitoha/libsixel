#!/bin/sh
# Ensure ambiguous select-color prefix is rejected.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_jpg="${images_dir}/snake.jpg"
require_file "${snake_jpg}"

if run_img2sixel -sa "${snake_jpg}" >/dev/null 2>>"${log_file}"; then
    fail 1 "ambiguous select-color prefix accepted"
else
    pass 1 "ambiguous select-color prefix rejected"
fi

exit "${status}"
