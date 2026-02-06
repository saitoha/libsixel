#!/bin/sh
# Inspect BMP while applying palette and scaling filters.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_bmp="${images_dir}/snake.bmp"
target_txt="${ARTIFACT_LOCAL_DIR}/bmp-inspection.txt"

if run_img2sixel -I -v -w200 -hauto -c100x1000+40+20 -qlow -dnone \
        -rhamming -thls "${snake_bmp}" >"${target_txt}"; then
    pass 1 "BMP inspection with filters succeeds"
else
    fail 1 "BMP inspection with filters fails"
fi

exit "${status}"
