#!/bin/sh
# Inspect Sixel with alternate ordered dither configuration.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_six="${images_dir}/snake.six"
target_txt="${ARTIFACT_LOCAL_DIR}/sixel-inspection-alt-dither.txt"

run_img2sixel -I -da_dither -w100 "${snake_six}" >"${target_txt}" || {
    fail 1 "alternate ordered dither inspection fails"
    exit "${status}"
}

pass 1 "alternate ordered dither inspection works"
exit "${status}"
