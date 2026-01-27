#!/bin/sh
# Ensure filename-driven PNG output uses correct header.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${images_dir}/snake.jpg"
filename_png="${tmp_dir}/snake-filename.png"

require_file "${snake_jpg}"

if run_img2sixel -o "${filename_png}" "${snake_jpg}" 2>>"${log_file}"; then
    header=$(od -An -tx1 -N8 "${filename_png}" | tr -d ' \n')
    if [ "${header}" = "89504e470d0a1a0a" ]; then
        pass 1 "filename-driven PNG output uses PNG header"
    else
        fail 1 "filename-driven PNG header incorrect"
    fi
else
    fail 1 "filename-driven PNG conversion failed"
fi

exit "${status}"
