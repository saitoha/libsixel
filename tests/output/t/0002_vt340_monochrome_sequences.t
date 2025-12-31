#!/bin/sh
# Verify VT340 monochrome control sequences are emitted.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_tga="${images_dir}/snake.tga"
target_sixel="${output_dir}/vt340-mono.sixel"

require_file "${snake_tga}"

if run_img2sixel -bvt340mono "${snake_tga}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "VT340 monochrome control sequences emitted"
else
    fail 1 "VT340 monochrome control failed"
fi

exit "${status}"
