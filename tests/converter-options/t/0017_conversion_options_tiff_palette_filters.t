#!/bin/sh
# Validate TIFF conversion with palette and filter options.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_tiff="${images_dir}/snake.tiff"
require_file "${snake_tiff}"

target_sixel="${tmp_dir}/snake-tiff.sixel"

if run_img2sixel -p200 -8 -scenter -Brgb:0/f/A -h100 -qfull -rhan -dstucki \
    -thls "${snake_tiff}" -o/dev/null 2>>"${log_file}"; then
    pass 1 "TIFF conversion with palette controls succeeded"
else
    fail 1 "TIFF conversion with palette controls failed"
fi

exit "${status}"
