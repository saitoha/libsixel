#!/bin/sh
# Inspect Sixel with alternate ordered dither configuration.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_six="${images_dir}/snake.six"
target_txt="${output_dir}/sixel-inspection-alt-dither.txt"

require_file "${snake_six}"

if run_img2sixel -I -da_dither -w100 "${snake_six}" \
        >"${target_txt}"; then
    pass 1 "alternate ordered dither inspection works"
else
    fail 1 "alternate ordered dither inspection fails"
fi

exit "${status}"
