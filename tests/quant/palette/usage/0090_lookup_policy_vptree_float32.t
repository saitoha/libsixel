#!/bin/sh
# Exercise the float32 VP-tree lookup policy through img2sixel options.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/vptree-float32.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --lookup-policy=vptree --precision=float32 -p 16 -d none \
        -o "${output_sixel}" "${snake_png}" || {
    echo "not ok" 1 - "float32 VP-tree lookup policy failed"
    exit 0
}

echo "ok" 1 - "float32 VP-tree lookup policy completes"

exit 0
