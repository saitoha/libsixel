#!/bin/sh
# Check inspection mode with diffusion and background colour.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_ppm="${images_dir}/snake.ppm"

target_txt="${ARTIFACT_LOCAL_DIR}/inspection.txt"

if ! run_img2sixel -I -dstucki -thls -B"#a0B030" "${snake_ppm}" >"${target_txt}"; then
    fail 1 "inspection with diffusion failed"
    exit "${status}"
fi

pass 1 "inspection with diffusion and background works"
exit "${status}"
