#!/bin/sh
# Confirm prefixed PNG output is created via png: scheme.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_jpg="${images_dir}/snake.jpg"
prefixed_png="${tmp_dir}/snake-prefixed.png"

require_file "${snake_jpg}"

if run_img2sixel -o "png:${prefixed_png}" "${snake_jpg}" 2>>"${log_file}"; then
    if [ -s "${prefixed_png}" ]; then
        pass 1 "prefixed PNG output created"
    else
        fail 1 "prefixed PNG output missing"
    fi
else
    fail 1 "prefixed PNG conversion failed"
fi

exit "${status}"
