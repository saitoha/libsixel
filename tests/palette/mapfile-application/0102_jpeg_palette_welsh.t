#!/bin/sh
# Convert JPEG using external palette and Welsh filter.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

egret_jpg="${TOP_SRCDIR}/images/egret.jpg"
map8_png="${TOP_SRCDIR}/images/map8.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/jpeg-welsh.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${map8_png}" -w200 -fau -rwelsh "${egret_jpg}" >"${target_sixel}" || {
    echo "not ok" 1 - "JPEG palette Welsh conversion fails"
    exit 0
}

echo "ok" 1 - "JPEG conversion using palette and Welsh filter"

exit 0
