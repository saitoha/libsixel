#!/bin/sh
# Inspect BMP while applying palette and scaling filters.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_bmp="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
target_txt="${ARTIFACT_LOCAL_DIR}/bmp-inspection.txt"

run_img2sixel -I -v -w200 -hauto -c100x1000+40+20 -qlow -dnone         -rhamming -thls "${snake_bmp}" >"${target_txt}" || {
    fail 1 "BMP inspection with filters fails"
    exit 0
}

pass 1 "BMP inspection with filters succeeds"

exit 0
