#!/bin/sh
# Validate scaling with histogram selection and background colour.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
snake_scaling="${ARTIFACT_LOCAL_DIR}/snake-scaling.sixel"

run_img2sixel -w50% -h150% -dfs -Bblue -thls -shist <"${snake_jpg}" \
    | tee "${snake_scaling}" >/dev/null || {
    fail 1 "scaling with histogram and background failed"
    exit 0
}

pass 1 "scaling with histogram and background succeeded"

exit 0
