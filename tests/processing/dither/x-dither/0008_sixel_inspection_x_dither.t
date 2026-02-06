#!/bin/sh
# Inspect Sixel with X ordered dither configuration.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_six="${images_dir}/snake.six"
target_txt="${ARTIFACT_LOCAL_DIR}/sixel-inspection-x-dither.txt"

run_img2sixel -I -dx_dither -h100 "${snake_six}" >"${target_txt}" || {
    fail 1 "X ordered dither inspection fails"
    exit "${status}"
}

pass 1 "X ordered dither inspection works"
exit "${status}"
