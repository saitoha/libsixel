#!/bin/sh
# Inspect PPM while applying aggressive scaling and filters.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_ppm="${images_dir}/snake.ppm"
target_txt="${ARTIFACT_LOCAL_DIR}/ppm-inspection.txt"

if run_img2sixel -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs \
        -rbilinear -trgb "${snake_ppm}" >"${target_txt}"; then
    pass 1 "PPM inspection tolerates aggressive scaling"
else
    fail 1 "PPM inspection with scaling fails"
fi

exit "${status}"
