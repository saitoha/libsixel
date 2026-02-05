#!/bin/sh
# Verify that img2sixel exits successfully when -O/--ormode is used.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${images_dir}/snake.jpg"
output_sixel="${tmp_dir}/snake-ormode.sixel"



# LSQA cannot read ormode sixel output, so only check for a clean exit.
if run_img2sixel -O --outfile="${output_sixel}" <"${snake_jpg}" \
; then
    pass 1 "ormode option exits successfully"
else
    fail 1 "ormode option failed to run"
fi

exit "${status}"
