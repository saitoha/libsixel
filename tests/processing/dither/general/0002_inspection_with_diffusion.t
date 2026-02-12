#!/bin/sh
# Check inspection mode with diffusion and background colour.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_ppm="${top_srcdir}/tests/data/inputs/small.ppm"

target_txt="${ARTIFACT_LOCAL_DIR}/inspection.txt"

run_img2sixel -I -dstucki -thls -B"#a0B030" "${snake_ppm}" >"${target_txt}" || {
    fail 1 "inspection with diffusion failed"
    exit 0
}

pass 1 "inspection with diffusion and background works"
exit 0
