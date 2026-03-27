#!/bin/sh
# Inspect PPM while applying aggressive scaling and filters.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
target_txt="${ARTIFACT_LOCAL_DIR}/ppm-inspection.txt"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs \
        -rbilinear -trgb "${snake_ppm}" >"${target_txt}" || {
    echo "not ok" 1 - "PPM inspection with scaling fails"
    exit 0
}

echo "ok" 1 - "PPM inspection tolerates aggressive scaling"

exit 0
