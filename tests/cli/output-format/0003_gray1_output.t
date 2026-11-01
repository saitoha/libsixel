#!/bin/sh
# Ensure 1-bit grayscale output succeeds.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_tga="${TOP_SRCDIR}/tests/data/inputs/snake_64.tga"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray1.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bgray1 -w120 "${snake_tga}" >"${target_sixel}" || {
    echo "not ok" 1 - "1-bit grayscale output fails"
    exit 0
}

echo "ok" 1 - "1-bit grayscale output succeeds"

exit 0
