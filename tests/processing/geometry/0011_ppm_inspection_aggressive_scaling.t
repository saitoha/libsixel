#!/bin/sh
# Inspect PPM while applying aggressive scaling and filters.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
target_txt="${ARTIFACT_LOCAL_DIR}/ppm-inspection.txt"

run_img2sixel -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs \
        -rbilinear -trgb "${snake_ppm}" >"${target_txt}" || {
    fail 1 "PPM inspection with scaling fails"
    exit 0
}

pass 1 "PPM inspection tolerates aggressive scaling"

exit 0
