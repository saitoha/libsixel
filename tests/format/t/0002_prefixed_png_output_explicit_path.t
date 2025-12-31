#!/bin/sh
# Confirm prefixed PNG output respects explicit path.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_jpg="${images_dir}/snake.jpg"
target_png="${tmp_dir}/snake-explicit.png"

require_file "${snake_jpg}"

if run_img2sixel -o "png:${target_png}" "${snake_jpg}" 2>>"${log_file}"; then
    if [ -s "${target_png}" ]; then
        pass 1 "prefixed PNG writes to explicit path"
    else
        fail 1 "prefixed PNG did not produce file"
    fi
else
    fail 1 "prefixed PNG conversion failed"
fi

exit "${status}"
