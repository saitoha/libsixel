#!/bin/sh
# Validate scaling with histogram selection and background colour.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
snake_scaling="${ARTIFACT_LOCAL_DIR}/snake-scaling.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -w50% -h150% -dfs -Bblue -thls -shist <"${snake_jpg}" \
    | tee "${snake_scaling}" >/dev/null || {
    echo "not ok" 1 - "scaling with histogram and background failed"
    exit 0
}

echo "ok" 1 - "scaling with histogram and background succeeded"

exit 0
