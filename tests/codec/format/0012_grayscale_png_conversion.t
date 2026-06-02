#!/bin/sh
# Convert grayscale PNG without palette overrides.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_gray_png="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-gray.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray-png.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "${snake_gray_png}" >"${target_sixel}" || {
    echo "not ok" 1 - "grayscale PNG conversion fails"
    exit 0
}

echo "ok" 1 - "grayscale PNG conversion succeeds"

exit 0
