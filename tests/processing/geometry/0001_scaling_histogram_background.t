#!/bin/sh
# Validate scaling with histogram selection and background colour.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
snake_scaling="${ARTIFACT_LOCAL_DIR}/snake-scaling.sixel"

if run_img2sixel -w50% -h150% -dfs -Bblue -thls -shist <"${snake_jpg}" \
    | tee "${snake_scaling}" >/dev/null; then
    pass 1 "scaling with histogram and background succeeded"
else
    fail 1 "scaling with histogram and background failed"
fi

exit "${status}"
